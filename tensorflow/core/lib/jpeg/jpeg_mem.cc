/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// This file defines functions to compress and uncompress JPEG data
// to and from memory, as well as some direct manipulations of JPEG string

#define RLBOX_SINGLE_THREADED_INVOCATIONS
//#define RLBOX_USE_STATIC_CALLS() rlbox_noop_sandbox_lookup_symbol

//#include "RLBOX/rlbox_noop_sandbox.hpp"
#include "RLBOX/rlbox_wasm2c_sandbox.hpp"

using sandbox_type_t = rlbox::rlbox_wasm2c_sandbox;

#include "RLBOX/rlbox.hpp"
using namespace rlbox;

#include "tensorflow/core/lib/jpeg/jpeg_mem.h"

#include <setjmp.h>
#include <string.h>
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <iostream>

#include "tensorflow/core/lib/jpeg/jpeg_handle.h"
#include "tensorflow/core/platform/dynamic_annotations.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/mem.h"
#include "tensorflow/core/platform/types.h"

template<typename T>
using tainted_img = rlbox::tainted<T, sandbox_type_t>;

#include "lib_struct_file.h"

rlbox_load_structs_from_library(jpeglib);

namespace tensorflow {
namespace jpeg {

// -----------------------------------------------------------------------------
// Decompression

namespace {

enum JPEGErrors {
  JPEGERRORS_OK,
  JPEGERRORS_UNEXPECTED_END_OF_DATA,
  JPEGERRORS_BAD_PARAM
};

// Prevent bad compiler behavior in ASAN mode by wrapping most of the
// arguments in a struct.
class FewerArgsForCompiler {
 public:
  FewerArgsForCompiler(int datasize, const UncompressFlags& flags,
                       int64_t* nwarn,
                       std::function<uint8*(int, int, int)> allocate_output)
      : datasize_(datasize),
        flags_(flags),
        pnwarn_(nwarn),
        allocate_output_(std::move(allocate_output)),
        height_read_(0),
        height_(0),
        stride_(0) {
    if (pnwarn_ != nullptr) *pnwarn_ = 0;
  }

  const int datasize_;
  const UncompressFlags flags_;
  int64_t* const pnwarn_;
  std::function<uint8*(int, int, int)> allocate_output_;
  int height_read_;  // number of scanline lines successfully read
  int height_;
  int stride_;
};

// Check whether the crop window is valid, assuming crop is true.
bool IsCropWindowValid(const UncompressFlags& flags, int input_image_width,
                       int input_image_height) {
  // Crop window is valid only if it is non zero and all the window region is
  // within the original image.
  return flags.crop_width > 0 && flags.crop_height > 0 && flags.crop_x >= 0 &&
         flags.crop_y >= 0 &&
         flags.crop_y + flags.crop_height <= input_image_height &&
         flags.crop_x + flags.crop_width <= input_image_width;
}

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
// If in fuzzing mode, don't print any error message as that slows down fuzzing.
// See also http://llvm.org/docs/LibFuzzer.html#fuzzer-friendly-build-mode
void no_print(j_common_ptr cinfo) {}
#endif

// Callback function for error exit
void exit_error_callback(rlbox_sandbox<sandbox_type_t> &sandbox, tainted_img<j_common_ptr> p_cinfo) {
  
  auto sandbox_mem_loc = sandbox.malloc_in_sandbox<jmp_buf*>(sizeof(jmp_buf));
  *sandbox_mem_loc = p_cinfo->client_data;
  tainted_img<jmp_buf*> app_ptr_tainted_reread = *sandbox_mem_loc;
  jmp_buf* jpeg_jmpbuf = sandbox.lookup_app_ptr(app_ptr_tainted_reread);
  //auto cinfo = p_cinfo.UNSAFE_unverified();
  // Seems dangerous, like an abritrary reader
 // (*cinfo->err->output_message)(cinfo);
 
 // jmp_buf *jpeg_jmpbuf = reinterpret_cast<jmp_buf *>(cinfo->client_data);
 // jpeg_destroy(cinfo);
  sandbox.invoke_sandbox_function(jpeg_destroy, p_cinfo);

  // Return Control to the setjmp point
  longjmp(*jpeg_jmpbuf, 1);
  sandbox.free_in_sandbox(sandbox_mem_loc);
}

uint8* UncompressLow(const void* srcdata, FewerArgsForCompiler* argball) {

  rlbox_sandbox<sandbox_type_t> sandbox;
  sandbox.create_sandbox("libWasmjpeg-turbo.so");

  // unpack the argball
  const int datasize = argball->datasize_;
  const auto& flags = argball->flags_;
  const int ratio = flags.ratio;
  int components = flags.components;
  int64_t* const nwarn = argball->pnwarn_;  // may be NULL
  
  // Stride is used multiple times in decompression to increment
  // array pointers, so we make a local tainted copy
  tainted_img<int> tainted_stride = flags.stride;   
  
  // Can't decode if the ratio is not recognized by libjpeg
  if ((ratio != 1) && (ratio != 2) && (ratio != 4) && (ratio != 8)) {
    sandbox.destroy_sandbox();
    return nullptr;
  }

  // Channels must be autodetect, grayscale, or rgb.
  if (!(components == 0 || components == 1 || components == 3)) {
    sandbox.destroy_sandbox();
    return nullptr;
  }

  // if empty image, return
  if (datasize == 0 || srcdata == nullptr) {
      sandbox.destroy_sandbox();
      return nullptr;
  }
  auto unchecked_srcdata = sandbox.malloc_in_sandbox<unsigned char>(datasize * sizeof(unsigned char));
  memcpy(sandbox, unchecked_srcdata, srcdata, datasize);

  // Declare temporary buffer pointer here so that we can free on error paths
  auto p_tempdata = sandbox.malloc_in_sandbox<JSAMPLE>(sizeof(JSAMPLE));

  // Initialize libjpeg structures to have a memory source
  // Modify the usual jpeg error manager to catch fatal errors.
  JPEGErrors error = JPEGERRORS_OK;
  auto p_cinfo = sandbox.malloc_in_sandbox<jpeg_decompress_struct>(sizeof(jpeg_decompress_struct));
  auto p_jerr = sandbox.malloc_in_sandbox<jpeg_error_mgr>(sizeof(jpeg_error_mgr));
  jmp_buf jpeg_jmpbuf;

  auto sandbox_mem_loc = sandbox.malloc_in_sandbox<jmp_buf*>(sizeof(jmp_buf));
  p_cinfo->err  = sandbox.invoke_sandbox_function(jpeg_std_error, p_jerr);

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  jerr->output_message = no_print;
#endif
  app_pointer<jmp_buf*, sandbox_type_t> app_ptr = sandbox.get_app_pointer(&jpeg_jmpbuf);
  tainted_img<jmp_buf*> app_ptr_tainted = app_ptr.to_tainted();

  // Override JPEG error exit using jmp_buf
  p_cinfo->client_data = app_ptr_tainted;
  auto callback = sandbox.register_callback(exit_error_callback);
  p_jerr->error_exit = callback;
  *sandbox_mem_loc = app_ptr_tainted;
  tainted_img<jmp_buf*> app_ptr_tainted_reread = *sandbox_mem_loc;
  jmp_buf* original_jpeg_jmpbuf = sandbox.lookup_app_ptr(app_ptr_tainted_reread);
  if (setjmp(*original_jpeg_jmpbuf)) {
    sandbox.free_in_sandbox(unchecked_srcdata);
    sandbox.free_in_sandbox(p_tempdata);
    sandbox.free_in_sandbox(p_cinfo);
    sandbox.free_in_sandbox(p_jerr);
    callback.unregister();
    app_ptr.unregister();
    sandbox.destroy_sandbox();
    return nullptr;
  }

  // Initialize JPEG Decompression Object
  sandbox.invoke_sandbox_function(jpeg_CreateDecompress, p_cinfo, JPEG_LIB_VERSION, (size_t) sizeof(struct jpeg_decompress_struct));
  sandbox.invoke_sandbox_function(SetSrc, p_cinfo, unchecked_srcdata, datasize, flags.try_recover_truncated_jpeg);
 
  // Read File Paramters
  sandbox.invoke_sandbox_function(jpeg_read_header, p_cinfo, TRUE);
  std::cout << "Made it 207" << std::endl;  
  // Set components automatically if desired, autoconverting cmyk to rgb.
  
  // We copy and verify here because we handle all cases in the switch statement below
  if (components == 0) {components = std::min(p_cinfo->num_components.copy_and_verify([] (int val) {
              return val;
              }), 3);
  }
  // set grayscale and ratio parameters
  switch (components) {
    case 1:
      {
      p_cinfo->out_color_space = JCS_GRAYSCALE;
      break;
    }
    case 3:
      {
      auto color_space = p_cinfo->jpeg_color_space.copy_and_verify([] (J_COLOR_SPACE jpeg_color_space) {return jpeg_color_space; });
        if (color_space == JCS_CMYK || color_space == JCS_YCCK){
        // Always use cmyk for output in a 4 channel jpeg. libjpeg has a
        // built-in decoder.  We will further convert to rgb below.
        p_cinfo->out_color_space = JCS_CMYK;
      } else {
        p_cinfo->out_color_space = JCS_RGB;
      }
      break;
    }
    default:
      {
      LOG(ERROR) << " Invalid components value " << components << std::endl;
      sandbox.invoke_sandbox_function(jpeg_destroy_decompress, p_cinfo);
      sandbox.free_in_sandbox(unchecked_srcdata);
      sandbox.free_in_sandbox(p_tempdata);
      sandbox.free_in_sandbox(p_cinfo);
      sandbox.free_in_sandbox(p_jerr);
      callback.unregister();
      app_ptr.unregister();
      sandbox.destroy_sandbox();
      return nullptr;
    }
  }
  p_cinfo->do_fancy_upsampling = boolean(flags.fancy_upscaling);
  p_cinfo->scale_num = 1;
  p_cinfo->scale_denom = ratio;
  p_cinfo->dct_method = flags.dct_method;

  // Determine the output image size before attempting decompress to prevent
  // OOM'ing during the decompress
  sandbox.invoke_sandbox_function(jpeg_calc_output_dimensions, p_cinfo);
  // Some of the internal routines do not gracefully handle ridiculously
  // large images, so fail fast.
  //
  auto tainted_total_size = p_cinfo->output_width * p_cinfo->output_height * p_cinfo->num_components;

  // Some of the internal routines do not gracefully handle ridiculously
  // large images, so fail fast.
  if (p_cinfo->output_width.unverified_safe_because("This is just a pre-check before we start decompress") <= 0 
          || p_cinfo->output_height.unverified_safe_because("This is just a pre-check before we start decompress") <= 0) {
    LOG(ERROR) << "Invalid image size: " << p_cinfo->output_width.unverified_safe_because("Error Logging") << " x "
               << p_cinfo->output_height.unverified_safe_because("Error Logging");
    sandbox.invoke_sandbox_function(jpeg_destroy_decompress, p_cinfo);
    sandbox.free_in_sandbox(unchecked_srcdata);
    sandbox.free_in_sandbox(p_tempdata);
    sandbox.free_in_sandbox(p_cinfo);
    sandbox.free_in_sandbox(p_jerr);
    callback.unregister();
    app_ptr.unregister();
    return nullptr;
  }
  if (tainted_total_size.unverified_safe_because("This is just a pre-check for large image before we start decompress") >= (1LL << 29)) {
    LOG(ERROR) << "Image too large: " << tainted_total_size.unverified_safe_because("We are error logging");
    sandbox.invoke_sandbox_function(jpeg_destroy_decompress, p_cinfo);
    sandbox.free_in_sandbox(unchecked_srcdata);
    sandbox.free_in_sandbox(p_tempdata);
    sandbox.free_in_sandbox(p_cinfo);
    sandbox.free_in_sandbox(p_jerr);
    callback.unregister();
    app_ptr.unregister();
    return nullptr;
  }

  sandbox.invoke_sandbox_function(jpeg_start_decompress, p_cinfo);

  tainted_img<JDIMENSION> tainted_target_output_width = p_cinfo->output_width;
  tainted_img<JDIMENSION> tainted_target_output_height = p_cinfo->output_height;
  auto p_skipped_scanlines = sandbox.malloc_in_sandbox<JDIMENSION>(sizeof(JDIMENSION));
  *p_skipped_scanlines = 0;
  
#if defined(LIBJPEG_TURBO_VERSION)
  if (flags.crop) {
    // Update target output height and width based on crop window.
    tainted_target_output_height = flags.crop_height;
    tainted_target_output_width = flags.crop_width;

    // So far, cinfo holds the original input image information.
    if (!IsCropWindowValid(flags, p_cinfo->output_width.unverified_safe_because("Crop width will still be moved to sandbox after"), 
                p_cinfo->output_height.unverified_safe_because("Crop Height will still be moved to sandbox after"))) {
      LOG(ERROR) << "Invalid crop window: x=" << flags.crop_x
                 << ", y=" << flags.crop_y << ", w=" << flags.crop_width
                 << ", h=" << flags.crop_height
                 << " for image_width: " << p_cinfo->output_width.unverified_safe_because("Printing Crop Size Error")
                 << " and image_height: " << p_cinfo->output_height.unverified_safe_because("Printing Crop Size Error");
      sandbox.invoke_sandbox_function(jpeg_destroy_decompress, p_cinfo);
      sandbox.free_in_sandbox(p_tempdata);
      sandbox.free_in_sandbox(p_cinfo);
      sandbox.free_in_sandbox(p_jerr);
      sandbox.destroy_sandbox();
      return nullptr;
    }

    // Update cinfo.output_width. It is tricky that cinfo.output_width must
    // fall on an Minimum Coded Unit (MCU) boundary; if it doesn't, then it will
    // be moved left to the nearest MCU boundary, and width will be increased
    // accordingly. Therefore, the final cinfo.crop_width might differ from the
    // given flags.crop_width. Please see libjpeg library for details.
    auto p_crop_width = sandbox.malloc_in_sandbox<JDIMENSION>(sizeof(JDIMENSION));
    *p_crop_width = flags.crop_width;
    auto p_crop_x = sandbox.malloc_in_sandbox<JDIMENSION>(sizeof(JDIMENSION));
    *p_crop_x = flags.crop_x;

    sandbox.invoke_sandbox_function(jpeg_crop_scanline, p_cinfo, p_crop_x, p_crop_width);
    // Update cinfo.output_scanline.
    *p_skipped_scanlines = sandbox.invoke_sandbox_function(jpeg_skip_scanlines, p_cinfo, flags.crop_y);
    CHECK_EQ((*p_skipped_scanlines).unverified_safe_because("We are error checking"), flags.crop_y);
  }
#endif

  // check for compatible stride
  
  // We have already verified these values
  auto tainted_min_stride = tainted_target_output_width * components * sizeof(JSAMPLE);

  // Stride is a primitive parameter passed into this function
  if (tainted_stride.unverified_safe_because("We will check stride later") == 0) {
    tainted_stride = tainted_min_stride.unverified_safe_because("We are setting tainted value");
  } else if (tainted_stride.unverified_safe_because("Error case will be verified later") < 
          tainted_min_stride.unverified_safe_because("Error Case will be verified later")) {
    LOG(ERROR) << "Incompatible stride: " << tainted_stride.unverified_safe_because("Error Logging")
        << " < " << tainted_min_stride.unverified_safe_because("Error Logging");
    sandbox.invoke_sandbox_function(jpeg_destroy_decompress, p_cinfo);
    sandbox.free_in_sandbox(unchecked_srcdata);
    sandbox.free_in_sandbox(p_tempdata);
    sandbox.free_in_sandbox(p_cinfo);
    sandbox.free_in_sandbox(p_jerr);
    callback.unregister();
    app_ptr.unregister();
    sandbox.destroy_sandbox();
    return nullptr;
  }

  // We verify target_output_height, target_output_width, and stride
  // here because we need them to allocate buffer size. We will need
  // to know these values when we copy the buffer back to application
  // memory.
  JDIMENSION target_output_width = p_cinfo->output_width.copy_and_verify([](JDIMENSION width) {
          if(width >0) {
            return width;
          }
          LOG(ERROR) << "Invalid image width: " << width;
          });
  JDIMENSION target_output_height = p_cinfo->output_height.copy_and_verify([](JDIMENSION height) {
          if(height >0) {
            return height;
          }
          LOG(ERROR) << "Invalid image height: " << height;
          });
  auto total_size = target_output_width * target_output_height * components;
  if (total_size >= (1LL << 29)) {
    LOG(ERROR) << "Image too large: " << total_size;
    sandbox.invoke_sandbox_function(jpeg_destroy_decompress, p_cinfo);
    sandbox.free_in_sandbox(unchecked_srcdata);
    sandbox.free_in_sandbox(p_tempdata);
    sandbox.free_in_sandbox(p_cinfo);
    sandbox.free_in_sandbox(p_jerr);
    callback.unregister();
    app_ptr.unregister();
    sandbox.destroy_sandbox();
    return nullptr;
  }
  auto stride = tainted_stride.copy_and_verify([&target_output_width, &components](int stride) {
          if(stride >= (target_output_width * components * sizeof(JSAMPLE))) {
            return stride;
          }
          LOG(ERROR) << "Incompatible stride: " << stride;
          });

  // Remember stride and height for use in Uncompress
  argball->height_ = target_output_height;
  argball->stride_ = stride;

   uint8* dstdata = nullptr;
#if !defined(LIBJPEG_TURBO_VERSION)
  if (flags.crop) {
    auto buf_size = stride * target_output_height;
    dstdata = new JSAMPLE[stride * target_output_height];
    auto p_dstdata = sandbox.malloc_in_sandbox<JSAMPLE>(sizeof(JSAMPLE) * stride * target_output_height);
  } else {
    auto buf_size = target_output_width * target_output_height * components;
    dstdata = argball->allocate_output_(target_output_width,target_output_height, components);
    auto p_dstdata = sandbox.malloc_in_sandbox<JSAMPLE>(sizeof(JSAMPLE) * target_output_width * target_output_height * components);
  }
#else
    auto buf_size = target_output_width * target_output_height * components;
    dstdata = argball->allocate_output_(target_output_width, target_output_height, components);
    auto p_dstdata = sandbox.malloc_in_sandbox<JSAMPLE>(sizeof(JSAMPLE) * target_output_width * target_output_height * components);
#endif
  if (p_dstdata == nullptr || dstdata == nullptr) {
    sandbox.invoke_sandbox_function(jpeg_destroy_decompress, p_cinfo);
    sandbox.free_in_sandbox(unchecked_srcdata);
    sandbox.free_in_sandbox(p_tempdata);
    sandbox.free_in_sandbox(p_cinfo);
    sandbox.free_in_sandbox(p_jerr);
    callback.unregister();
    app_ptr.unregister();
    sandbox.destroy_sandbox();
    delete[] dstdata;
    return nullptr;
  }

  // jpeg_read_scanlines requires the buffers to be allocated based on
  // cinfo.output_width, but the target image width might be different if crop
  // is enabled and crop_width is not MCU aligned. In this case, we need to
  // realign the scanline output to achieve the exact cropping.  Notably, only
  // cinfo.output_width needs to fall on MCU boundary, while cinfo.output_height
  // has no such constraint.

  const bool need_realign_cropped_scanline = 
      target_output_width != p_cinfo->output_width.unverified_safe_because("We are just setting realign flag, and will check for realign later");
  const bool use_cmyk = (p_cinfo->out_color_space.unverified_safe_because("We are setting cmyk flag and will check it later") == JCS_CMYK);

  if (use_cmyk) {
    // Temporary buffer used for CMYK -> RGB conversion.
      sandbox.free_in_sandbox(p_tempdata);
      p_tempdata = sandbox.malloc_in_sandbox<JSAMPLE>(sizeof(JSAMPLE) 
              * p_cinfo->output_width.unverified_safe_because("Temporary Buffer for CYMK -> RGB Conversion") * 4);
  } else if (need_realign_cropped_scanline) {
    // Temporary buffer used for MCU-aligned scanline data.
    sandbox.free_in_sandbox(p_tempdata);
    p_tempdata = sandbox.malloc_in_sandbox<JSAMPLE>(sizeof(JSAMPLE) 
            * p_cinfo->output_width.unverified_safe_because("Any value is safe for allocation") * components);
  }

  // If there is an error reading a line, this aborts the reading.
  // Save the fraction of the image that has been read.
  argball->height_read_ = target_output_height;

  // These variables are just to avoid repeated computation in the loop.
  const int max_scanlines_to_read = (*p_skipped_scanlines).copy_and_verify([&flags] 
          (JDIMENSION skipped_scanlines) {
          if(flags.crop) {
            assert(skipped_scanlines === flags.crop_y);
            return skipped_scanlines;
          }
          return (unsigned int)0;
            }) + target_output_height;

  auto tainted_mcu_align_offset = (p_cinfo->output_width - target_output_width) * (use_cmyk ? 4 : components);

  // Temp buffer for reading one scanline at a time
  auto arr_p_tempdata = sandbox.malloc_in_sandbox<JSAMPLE*>(1 * sizeof(JSAMPLE*));
  *arr_p_tempdata = p_tempdata;

  auto arr_p_dstdata = sandbox.malloc_in_sandbox<JSAMPLE*>(1 * sizeof(JSAMPLE*));
  
  // Image Read Buffer
  auto tainted_output_read_buffer = sandbox.malloc_in_sandbox<JSAMPLE>();
  tainted_output_read_buffer = p_dstdata;

  // Pointer to start of buffer
  auto final_dst = sandbox.malloc_in_sandbox<JSAMPLE>();
  final_dst = p_dstdata;

  // This is our local variable of output_scanline. We will use it in the decompression loop
  // so we only have to verify once during each iteration. This avoids double fetch bugs.
  JDIMENSION output_scanline = p_cinfo->output_scanline.copy_and_verify([&max_scanlines_to_read]
              (JDIMENSION output_scanline) {
              assert(output_scanline < max_scanlines_to_read && output_scanline >= 0);
              return output_scanline;});

  while (output_scanline < max_scanlines_to_read) {
    *arr_p_dstdata = p_dstdata;
    int num_lines_read = 0;
    if (use_cmyk) {
      num_lines_read = sandbox.invoke_sandbox_function(jpeg_read_scanlines, p_cinfo, 
              arr_p_tempdata, 1).copy_and_verify([] (int val){return val;});
      if (num_lines_read > 0) {
        // Convert CMYK to RGB if scanline read succeeded.
        for (size_t i = 0; i < target_output_width; ++i) {
          tainted_img<unsigned int> offset = 4 * i;
          if (need_realign_cropped_scanline) {
            // Align the offset for MCU boundary.
            offset = offset + tainted_mcu_align_offset;
          const int c = p_tempdata[offset + 0].unverified_safe_because("We are just reading bytes");
          const int m = p_tempdata[offset + 1].unverified_safe_because("We are just reading bytes");
          const int y = p_tempdata[offset + 2].unverified_safe_because("We are just reading bytes");
          const int k = p_tempdata[offset + 3].unverified_safe_because("We are just reading bytes");
          int r, g, b;
          if (p_cinfo->saw_Adobe_marker.unverified_safe_because("Flag for what we write to read_buffer")) {
            r = (k * c) / 255;
            g = (k * m) / 255;
            b = (k * y) / 255;
          } else {
            r = (255 - k) * (255 - c) / 255;
            g = (255 - k) * (255 - m) / 255;
            b = (255 - k) * (255 - y) / 255;
          }
          p_dstdata[3 * i + 0] = r;
          p_dstdata[3 * i + 1] = g;
          p_dstdata[3 * i + 2] = b;
        }
      }
    } else if (need_realign_cropped_scanline) {
      num_lines_read = sandbox.invoke_sandbox_function(jpeg_read_scanlines, p_cinfo, 
              arr_p_tempdata, 1).copy_and_verify([] (int val){return val;});
    }
      if (num_lines_read > 0) {
        memcpy(sandbox, p_dstdata, p_tempdata + tainted_mcu_align_offset, tainted_min_stride);
      }
    } else {
      num_lines_read = sandbox.invoke_sandbox_function(jpeg_read_scanlines, p_cinfo, 
              arr_p_dstdata, 1).copy_and_verify([] (int val){return val;});
    }
      
    output_scanline = p_cinfo->output_scanline.copy_and_verify([&max_scanlines_to_read]
              (JDIMENSION output_scanline) {
              assert(output_scanline < max_scanlines_to_read && output_scanline >= 0);
              return output_scanline;});
    // Handle error cases
    if (num_lines_read == 0) {
      auto skipped_scanlines = (*p_skipped_scanlines).copy_and_verify([] (JDIMENSION skipped_scanlines)
              {assert(skipped_scanlines >= 0);
              return skipped_scanlines;
              });
      tainted_img<JDIMENSION>  tainted_height_read = output_scanline - skipped_scanlines;
      auto height_read = tainted_height_read.copy_and_verify([&target_output_height](JDIMENSION height_read) {
              assert(height_read < target_output_height && height_read >= 0);
              return height_read;
              });
      LOG(ERROR) << "Premature end of JPEG data. Stopped at line "
                 << height_read << "/"
                 << target_output_height;
      if (!flags.try_recover_truncated_jpeg) {
        argball->height_read_ = height_read;
        error = JPEGERRORS_UNEXPECTED_END_OF_DATA;
      } else {
        for (size_t line = output_scanline; line < max_scanlines_to_read; ++line) {
          if (line == 0) {
            // If even the first line is missing, fill with black color
            memset(sandbox, p_dstdata, 0, tainted_min_stride);
          } else {
            // else, just replicate the line above.
            memcpy(sandbox, p_dstdata, p_dstdata - stride, tainted_min_stride);
          }
          p_dstdata += stride;
        }
        argball->height_read_ =
            target_output_height;  // consider all lines as read
        // prevent error-on-exit in libjpeg:
        p_cinfo->output_scanline = max_scanlines_to_read;
      }
      break;
    }
    DCHECK_EQ(num_lines_read, 1);
    TF_ANNOTATE_MEMORY_IS_INITIALIZED(p_dstdata, tainted_min_stride);
    p_dstdata += stride;
  }
  
  // Free temp buffer
  sandbox.free_in_sandbox(p_tempdata);
  p_tempdata = nullptr;

#if defined(LIBJPEG_TURBO_VERSION)
  
  
  auto output_height = p_cinfo->output_height.copy_and_verify([&flags] (JDIMENSION output_height){
          assert(output_height - flags.crop_y - flags.crop_height >= 0);
          return output_height;
          });
          
  if (flags.crop && output_scanline < output_height) {
    // Skip the rest of scanlines, required by jpeg_destroy_decompress.
    sandbox.invoke_sandbox_function(jpeg_skip_scanlines, p_cinfo, output_height - flags.crop_y - flags.crop_height);
    // After this, cinfo.output_height must be equal to cinfo.output_height;
    // otherwise, jpeg_destroy_decompress would fail.
  }
#endif

  // Convert the RGB data to RGBA, with alpha set to 0xFF to indicate
  // opacity.
  // RGBRGBRGB... --> RGBARGBARGBA...
  if (components == 4) {
    // Start on the last line.
    auto scanlineptr = tainted_output_read_buffer + (target_output_height - 1) * stride; 
    const JSAMPLE kOpaque = -1;  // All ones appropriate for JSAMPLE.
    const int right_rgb = (target_output_width - 1) * 3;
    const int right_rgba = (target_output_width - 1) * 4;

    for (int y = target_output_height; y-- > 0;) {
      // We do all the transformations in place, going backwards for each row.
      auto rgb_pixel = scanlineptr + right_rgb;
      auto rgba_pixel = scanlineptr + right_rgba;
      scanlineptr -= stride;
      for (int x = target_output_width; x-- > 0;
           rgba_pixel -= 4, rgb_pixel -= 3) {
        // We copy the 3 bytes at rgb_pixel into the 4 bytes at rgba_pixel
        // The "a" channel is set to be opaque.
        rgba_pixel[3] = kOpaque;
        rgba_pixel[2] = rgb_pixel[2];
        rgba_pixel[1] = rgb_pixel[1];
        rgba_pixel[0] = rgb_pixel[0];
      }
    }
  }


  switch (components) {
    case 1:
      if (p_cinfo->output_components.unverified_safe_because("Logging Errors") != 1) {
        error = JPEGERRORS_BAD_PARAM;
      }
      break;
    case 3:
    case 4:
      if (p_cinfo->out_color_space.unverified_safe_because("Logging Errors") == JCS_CMYK) {
        if (p_cinfo->output_components.unverified_safe_because("Logging Errors") != 4) {
          error = JPEGERRORS_BAD_PARAM;
        }
      } else {
        if (p_cinfo->output_components.unverified_safe_because("Logging Errors") != 3) {
          error = JPEGERRORS_BAD_PARAM;
        }
      }
      break;
    default:
      // will never happen, should be caught by the previous switch
      LOG(ERROR) << "Invalid components value " << components << std::endl;
      sandbox.invoke_sandbox_function(jpeg_destroy_decompress, p_cinfo);
      sandbox.free_in_sandbox(unchecked_srcdata);
      sandbox.free_in_sandbox(p_tempdata);
      sandbox.free_in_sandbox(p_cinfo);
      sandbox.free_in_sandbox(p_jerr);
      callback.unregister();
      app_ptr.unregister();
      sandbox.destroy_sandbox();
      delete[] dstdata;
      return nullptr;
  }

  // save number of warnings if requested
  if (argball->pnwarn_ != nullptr) {
    *argball->pnwarn_ = p_cinfo->err->num_warnings.copy_and_verify([] (long num_warnings) { 
             assert(num_warnings >= 0);
             return num_warnings;});
  }

  // Handle errors in JPEG
  switch (error) {
    case JPEGERRORS_OK:
      sandbox.invoke_sandbox_function(jpeg_finish_decompress, p_cinfo);
      break;
    case JPEGERRORS_UNEXPECTED_END_OF_DATA:
    case JPEGERRORS_BAD_PARAM:
      jpeg_abort(reinterpret_cast<j_common_ptr>(p_cinfo.unverified_safe_pointer_because(1, "We are aborting")));
      break;
    default:
      LOG(ERROR) << "Unhandled case " << error;
      break;
  }
/*
#if !defined(LIBJPEG_TURBO_VERSION)
  // TODO(tanmingxing): delete all these code after migrating to libjpeg_turbo
  // for Windows.
  if (flags.crop) {
      // Update target output height and width based on crop window.
      target_output_height = flags.crop_height;
      target_output_width = flags.crop_width;
      if (!IsCropWindowValid(flags, p_cinfo->output_width.unverified_safe_because("Crop width will still be moved to sandbox after"), 
              p_cinfo->output_height.unverified_safe_because("Crop Height will still be moved to sandbox after"))) {
      LOG(ERROR) << "Invalid crop window: x=" << flags.crop_x
                 << ", y=" << flags.crop_y << ", w=" << target_output_width
                 << ", h=" << target_output_height
                 << " for image_width: " << p_cinfo->output_width.unverified_safe_because("Printing Crop Size Error")
                 << " and image_height: " << p_cinfo->output_height.unverified_safe_because("Printing Crop Size Error");
      sandbox.invoke_sandbox_function(jpeg_destroy_decompress, p_cinfo);
      sandbox.free_in_sandbox(p_tempdata);
      sandbox.free_in_sandbox(p_cinfo);
      sandbox.free_in_sandbox(p_jerr);
      sandbox.destroy_sandbox();
      return nullptr;
    }
    dstdata = argball->allocate_output_(target_output_width,
                                        target_output_height, components);
    auto p_dstdata = sandbox.malloc_in_sandbox<JSAMPLE>(sizeof(JSAMPLE) * target_output_width * target_output_height * components);
    auto tainted_output_read_buffer = p_dstdata;
    if (dstdata == nullptr || p_dst_data == nullptr) {
      delete[] dstdata;
      sandbox.invoke_sandbox_function(jpeg_destroy_decompress, p_cinfo);
      sandbox.free_in_sandbox(p_tempdata);
      sandbox.free_in_sandbox(p_cinfo);
      sandbox.free_in_sandbox(p_jerr);
      sandbox.destroy_sandbox();
      return nullptr;
    }
    // Update stride and hight for crop window.
    const int min_stride = target_output_width * components * sizeof(JSAMPLE);
    if (stride == 0) {
      stride = min_stride;
    } else if (stride < min_stride) {
      LOG(ERROR) << "Incompatible stride: " << stride << " < " << min_stride;
      sandbox.invoke_sandbox_function(jpeg_destroy_decompress, p_cinfo);
      sandbox.free_in_sandbox(p_tempdata);
      sandbox.free_in_sandbox(p_cinfo);
      sandbox.free_in_sandbox(p_jerr);
      sandbox.destroy_sandbox();
      return nullptr;
    }

    // Remember stride and height for use in Uncompress
    argball->height_ = target_output_height;
    argball->stride_ = stride;
    if (argball->height_read_ > target_output_height) {
      argball->height_read_ = target_output_height;
    }
    const int crop_offset = flags.crop_x * components * sizeof(JSAMPLE);
    auto full_image_ptr = p_dstdata + flags.crop_y * full_image_stride;
    auto crop_image_ptr = p_dstdata
    for (int i = 0; i < argball->height_read_; i++) {
      memcpy(sandbox, crop_image_ptr, full_image_ptr + crop_offset, min_stride);
      crop_image_ptr += stride;
      full_image_ptr += full_image_stride;
    }
  }
  
#endif
*/
  sandbox.invoke_sandbox_function(jpeg_destroy_decompress, p_cinfo);
  auto end_dstdata = dstdata;
  /*if(flags.crop) {
    auto dest = tainted_output_read_buffer.copy_and_verify_range([&buf_size, &target_output_height, 
            &stride](std::unique_ptr<unsigned char[]> buf) {
            return buf_size == (target_output_height * stride) ? std::move(buf) : nullptr;
            }, buf_size);
    for (int i = 0; i < target_output_height * stride; i++) {
        end_dstdata[i] = dest[i];
    }
  }
  else{
    auto dest = tainted_output_read_buffer.copy_and_verify_range([&buf_size, &target_output_width, 
            &target_output_height, &components](std::unique_ptr<unsigned char[]> buf) {
            return buf_size == (target_output_width * target_output_height * components) ? std::move(buf) : nullptr;
            }, buf_size);
    for (int i = 0; i < target_output_width * target_output_height * components; i++) {
        end_dstdata[i] = dest[i];
    }
  }*/
  auto dest = tainted_output_read_buffer.copy_and_verify_range([](std::unique_ptr<unsigned char[]> buf) {
          return std::move(buf);
          }, buf_size);
  for (int i = 0; i < buf_size; i++) {
      end_dstdata[i] = dest[i];
  }
  sandbox.free_in_sandbox(unchecked_srcdata);
  sandbox.free_in_sandbox(p_tempdata);
  sandbox.free_in_sandbox(p_cinfo);
  sandbox.free_in_sandbox(p_jerr);
  sandbox.free_in_sandbox(p_skipped_scanlines);
  sandbox.free_in_sandbox(final_dst);
  sandbox.free_in_sandbox(arr_p_dstdata);

  callback.unregister();
  app_ptr.unregister();
  sandbox.destroy_sandbox();
  std::cout << "Made it to the end" << std::endl;
  return dstdata;
}
}  // anonymous namespace

// -----------------------------------------------------------------------------
//  We do the apparently silly thing of packing 5 of the arguments
//  into a structure that is then passed to another routine
//  that does all the work.  The reason is that we want to catch
//  fatal JPEG library errors with setjmp/longjmp, and g++ and
//  associated libraries aren't good enough to guarantee that 7
//  parameters won't get clobbered by the longjmp.  So we help
//  it out a little.
uint8* Uncompress(const void* srcdata, int datasize,
                  const UncompressFlags& flags, int64_t* nwarn,
                  std::function<uint8*(int, int, int)> allocate_output) {
  FewerArgsForCompiler argball(datasize, flags, nwarn,
                               std::move(allocate_output));
  uint8* const dstdata = UncompressLow(srcdata, &argball);

  const float fraction_read =
      argball.height_ == 0
          ? 1.0
          : (static_cast<float>(argball.height_read_) / argball.height_);
  if (dstdata == nullptr ||
      fraction_read < std::min(1.0f, flags.min_acceptable_fraction)) {
    // Major failure, none or too-partial read returned; get out
    return nullptr;
  }

//   for (int i = 0; i < argball.height_read_; i++) {
  //      std::cout << dstdata[i] << std::endl;
   // }
  // If there was an error in reading the jpeg data,
  // set the unread pixels to black
  if (argball.height_read_ != argball.height_) {
    const int first_bad_line = argball.height_read_;
    uint8* start = dstdata + first_bad_line * argball.stride_;
    const int nbytes = (argball.height_ - first_bad_line) * argball.stride_;
    memset(static_cast<void*>(start), 0, nbytes);
  }

  return dstdata;
}

uint8* Uncompress(const void* srcdata, int datasize,
                  const UncompressFlags& flags, int* pwidth, int* pheight,
                  int* pcomponents, int64_t* nwarn) {
  uint8* buffer = nullptr;
  uint8* result =
      Uncompress(srcdata, datasize, flags, nwarn,
                 [=, &buffer](int width, int height, int components) {
                   if (pwidth != nullptr) *pwidth = width;
                   if (pheight != nullptr) *pheight = height;
                   if (pcomponents != nullptr) *pcomponents = components;
                   buffer = new uint8[height * width * components];
                   return buffer;
                 });
  if (!result) delete[] buffer;
  return result;
}

// ----------------------------------------------------------------------------
// Computes image information from jpeg header.
// Returns true on success; false on failure.
bool GetImageInfo(const void* srcdata, int datasize, int* width, int* height,
                  int* components) {
  // Init in case of failure
  if (width) *width = 0;
  if (height) *height = 0;
  if (components) *components = 0;

  // If empty image, return
  if (datasize == 0 || srcdata == nullptr) return false;

  // Initialize libjpeg structures to have a memory source
  // Modify the usual jpeg error manager to catch fatal errors.
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  jmp_buf jpeg_jmpbuf;
  cinfo.err = jpeg_std_error(&jerr);
  cinfo.client_data = &jpeg_jmpbuf;
  jerr.error_exit = CatchError;
  if (setjmp(jpeg_jmpbuf)) {
    return false;
  }

  // set up, read header, set image parameters, save size
  jpeg_create_decompress(&cinfo);
  SetSrc(&cinfo, srcdata, datasize, false);

  jpeg_read_header(&cinfo, TRUE);
  jpeg_calc_output_dimensions(&cinfo);
  if (width) *width = cinfo.output_width;
  if (height) *height = cinfo.output_height;
  if (components) *components = cinfo.output_components;

  jpeg_destroy_decompress(&cinfo);

  return true;
}
// -----------------------------------------------------------------------------
// Compression

namespace {
bool CompressInternal(const uint8* srcdata, int width, int height,
                      const CompressFlags& flags, tstring* output) {
  if (output == nullptr) {
    LOG(ERROR) << "Output buffer is null: ";
    return false;
  }

  output->clear();
  const int components = (static_cast<int>(flags.format) & 0xff);

  int64_t total_size =
      static_cast<int64_t>(width) * static_cast<int64_t>(height);
  // Some of the internal routines do not gracefully handle ridiculously
  // large images, so fail fast.
  if (width <= 0 || height <= 0) {
    LOG(ERROR) << "Invalid image size: " << width << " x " << height;
    return false;
  }
  if (total_size >= (1LL << 29)) {
    LOG(ERROR) << "Image too large: " << total_size;
    return false;
  }

  int in_stride = flags.stride;
  if (in_stride == 0) {
    in_stride = width * (static_cast<int>(flags.format) & 0xff);
  } else if (in_stride < width * components) {
    LOG(ERROR) << "Incompatible input stride";
    return false;
  }

  JOCTET* buffer = nullptr;

  // NOTE: for broader use xmp_metadata should be made a Unicode string
  CHECK(srcdata != nullptr);
  CHECK(output != nullptr);
  // This struct contains the JPEG compression parameters and pointers to
  // working space
  struct jpeg_compress_struct cinfo;
  // This struct represents a JPEG error handler.
  struct jpeg_error_mgr jerr;
  jmp_buf jpeg_jmpbuf;  // recovery point in case of error

  // Step 1: allocate and initialize JPEG compression object
  // Use the usual jpeg error manager.
  cinfo.err = jpeg_std_error(&jerr);
  cinfo.client_data = &jpeg_jmpbuf;
  jerr.error_exit = CatchError;
  if (setjmp(jpeg_jmpbuf)) {
    output->clear();
    delete[] buffer;
    return false;
  }

  jpeg_create_compress(&cinfo);

  // Step 2: specify data destination
  // We allocate a buffer of reasonable size. If we have a small image, just
  // estimate the size of the output using the number of bytes of the input.
  // If this is getting too big, we will append to the string by chunks of 1MB.
  // This seems like a reasonable compromise between performance and memory.
  int bufsize = std::min(width * height * components, 1 << 20);
  buffer = new JOCTET[bufsize];
  SetDest(&cinfo, buffer, bufsize, output);

  // Step 3: set parameters for compression
  cinfo.image_width = width;
  cinfo.image_height = height;
  switch (components) {
    case 1:
      cinfo.input_components = 1;
      cinfo.in_color_space = JCS_GRAYSCALE;
      break;
    case 3:
    case 4:
      cinfo.input_components = 3;
      cinfo.in_color_space = JCS_RGB;
      break;
    default:
      LOG(ERROR) << " Invalid components value " << components << std::endl;
      output->clear();
      delete[] buffer;
      return false;
  }
  jpeg_set_defaults(&cinfo);
  if (flags.optimize_jpeg_size) cinfo.optimize_coding = TRUE;

  cinfo.density_unit = flags.density_unit;  // JFIF code for pixel size units:
                                            // 1 = in, 2 = cm
  cinfo.X_density = flags.x_density;        // Horizontal pixel density
  cinfo.Y_density = flags.y_density;        // Vertical pixel density
  jpeg_set_quality(&cinfo, flags.quality, TRUE);

  if (flags.progressive) {
    jpeg_simple_progression(&cinfo);
  }

  if (!flags.chroma_downsampling) {
    // Turn off chroma subsampling (it is on by default).  For more details on
    // chroma subsampling, see http://en.wikipedia.org/wiki/Chroma_subsampling.
    for (int i = 0; i < cinfo.num_components; ++i) {
      cinfo.comp_info[i].h_samp_factor = 1;
      cinfo.comp_info[i].v_samp_factor = 1;
    }
  }

  jpeg_start_compress(&cinfo, TRUE);

  // Embed XMP metadata if any
  if (!flags.xmp_metadata.empty()) {
    // XMP metadata is embedded in the APP1 tag of JPEG and requires this
    // namespace header string (null-terminated)
    const string name_space = "http://ns.adobe.com/xap/1.0/";
    const int name_space_length = name_space.size();
    const int metadata_length = flags.xmp_metadata.size();
    const int packet_length = metadata_length + name_space_length + 1;
    std::unique_ptr<JOCTET[]> joctet_packet(new JOCTET[packet_length]);

    for (int i = 0; i < name_space_length; i++) {
      // Conversion char --> JOCTET
      joctet_packet[i] = name_space[i];
    }
    joctet_packet[name_space_length] = 0;  // null-terminate namespace string

    for (int i = 0; i < metadata_length; i++) {
      // Conversion char --> JOCTET
      joctet_packet[i + name_space_length + 1] = flags.xmp_metadata[i];
    }
    jpeg_write_marker(&cinfo, JPEG_APP0 + 1, joctet_packet.get(),
                      packet_length);
  }

  // JSAMPLEs per row in image_buffer
  std::unique_ptr<JSAMPLE[]> row_temp(
      new JSAMPLE[width * cinfo.input_components]);
  while (cinfo.next_scanline < cinfo.image_height) {
    JSAMPROW row_pointer[1];  // pointer to JSAMPLE row[s]
    const uint8* r = &srcdata[cinfo.next_scanline * in_stride];
    uint8* p = static_cast<uint8*>(row_temp.get());
    switch (flags.format) {
      case FORMAT_RGBA: {
        for (int i = 0; i < width; ++i, p += 3, r += 4) {
          p[0] = r[0];
          p[1] = r[1];
          p[2] = r[2];
        }
        row_pointer[0] = row_temp.get();
        break;
      }
      case FORMAT_ABGR: {
        for (int i = 0; i < width; ++i, p += 3, r += 4) {
          p[0] = r[3];
          p[1] = r[2];
          p[2] = r[1];
        }
        row_pointer[0] = row_temp.get();
        break;
      }
      default: {
        row_pointer[0] = reinterpret_cast<JSAMPLE*>(const_cast<JSAMPLE*>(r));
      }
    }
    CHECK_EQ(jpeg_write_scanlines(&cinfo, row_pointer, 1), 1u);
  }
  jpeg_finish_compress(&cinfo);

  // release JPEG compression object
  jpeg_destroy_compress(&cinfo);
  delete[] buffer;
  return true;
}

}  // anonymous namespace

// -----------------------------------------------------------------------------

bool Compress(const void* srcdata, int width, int height,
              const CompressFlags& flags, tstring* output) {
  return CompressInternal(static_cast<const uint8*>(srcdata), width, height,
                          flags, output);
}

tstring Compress(const void* srcdata, int width, int height,
                 const CompressFlags& flags) {
  tstring temp;
  CompressInternal(static_cast<const uint8*>(srcdata), width, height, flags,
                   &temp);
  // If CompressInternal fails, temp will be empty.
  return temp;
}

}  // namespace jpeg
}  // namespace tensorflow
