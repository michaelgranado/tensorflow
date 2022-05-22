# Description:
# JPEG handle and mem packages.

load(
    "//tensorflow:tensorflow.bzl",
    "if_android",
    "if_mobile",
    "tf_cc_test",
    "tf_copts",
)
load(
    "//tensorflow/core/platform:rules_cc.bzl",
    "cc_library",
)

package(
    default_visibility = ["//tensorflow/core:__pkg__"],
    features = ["-parse_headers"],
    licenses = ["notice"],
)

cc_library(
    name = "jpeg_internal",
    srcs = [
        "jpeg_handle.cc",
        "jpeg_handle2.cc",
        "jpeg_mem.cc",
        "//tensorflow/core/platform:jpeg_hdrs",
    ],
    hdrs = [
        "jpeg_handle.h",
        "jpeg_handle2.h",
        "jpeg_mem.h",
        "RLBOX/rlbox.hpp",
        "RLBOX/rlbox_app_pointer.hpp",
        "RLBOX/rlbox_conversion.hpp",
        "RLBOX/rlbox_dylib_sandbox.hpp",
        "RLBOX/rlbox_helpers.hpp",
        "RLBOX/rlbox_noop_sandbox.hpp",
        "RLBOX/rlbox_policy_types.hpp",
        "RLBOX/rlbox_range.hpp",
        "RLBOX/rlbox_sandbox.hpp",
        "RLBOX/rlbox_stdlib.hpp",
        "RLBOX/rlbox_stdlib_polyfill.hpp",
        "RLBOX/rlbox_struct_support.hpp",
        "RLBOX/rlbox_type_traits.hpp",
        "RLBOX/rlbox_types.hpp",
        "RLBOX/rlbox_unwrap.hpp",
        "RLBOX/rlbox_wrapper_traits.hpp",
        "RLBOX/rlbox_wasm2c_sandbox.hpp",
        "RLBOX/wasm-rt.h",
        "lib_struct_file.h",
    ],
    copts = tf_copts(),
    linkopts = select({
        "//tensorflow:freebsd": [],
        "//tensorflow:windows": [],
        "//conditions:default": ["-ldl"],
    }),
    deps = [
        "//tensorflow/core:lib",
        "//tensorflow/core:lib_internal",
        "//tensorflow/core/platform:jpeg",
        "@libjpeg_turbo//:jpeg",
    ],
)

cc_library(
    name = "portable_jpeg_internal",
    srcs = if_mobile([
        "jpeg_handle.cc",
        "jpeg_mem.cc",
        "//tensorflow/core/platform:jpeg_hdrs",
    ]),
    hdrs = [
        "jpeg_handle.h",
        "jpeg_mem.h",
        "//tensorflow/core/lib/core:legacy_lib_core_stringpiece_header",
        "//tensorflow/core/platform:jpeg_internal_hdrs",
        "//tensorflow/core/platform/default:integral_types.h",
        "//tensorflow/core/platform/default:logging.h",
    ],
    copts = tf_copts(),
    linkopts = if_android(["-ldl"]),
    deps = [
        "//tensorflow/core:core_stringpiece",
        "//tensorflow/core/platform:dynamic_annotations",
        "//tensorflow/core/platform:jpeg",
        "//tensorflow/core/platform:logging",
        "//tensorflow/core/platform:stringpiece",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
    ],
)

tf_cc_test(
    name = "lib_jpeg_jpeg_mem_unittest",
    srcs = ["jpeg_mem_unittest.cc"],
    data = ["//tensorflow/core/lib/jpeg/testdata"],
    deps = [
        ":jpeg_internal",
        "//tensorflow/core:lib",
        "//tensorflow/core:lib_internal",
        "//tensorflow/core:test",
        "//tensorflow/core:test_main",
        "@com_google_absl//absl/base",
    ],
)
