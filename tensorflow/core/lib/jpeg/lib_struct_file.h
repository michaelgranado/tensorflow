#define sandbox_fields_reflection_jpeglib_class_jpeg_error_mgr(f, g, ...) \
	f(void (*)(j_common_ptr), error_exit, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_common_ptr, int), emit_message, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_common_ptr), output_message, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_common_ptr, char *), format_message, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void (*)(j_common_ptr), reset_error_mgr, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, msg_code, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(char[80], msg_parm, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, trace_level, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(long, num_warnings, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(const char *const *, jpeg_message_table, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, last_jpeg_message, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(const char *const *, addon_message_table, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, first_addon_message, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, last_addon_message, FIELD_NORMAL, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_jpeglib_class_jpeg_decompress_struct(f, g, ...) \
	f(struct jpeg_error_mgr *, err, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_memory_mgr *, mem, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_progress_mgr *, progress, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(void *, client_data, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, is_decompressor, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, global_state, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_source_mgr *, src, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, image_width, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, image_height, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, num_components, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(J_COLOR_SPACE, jpeg_color_space, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(J_COLOR_SPACE, out_color_space, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, scale_num, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, scale_denom, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(double, output_gamma, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, buffered_image, FIELD_FREEZABLE, ##__VA_ARGS__) \
	g() \
	f(int, raw_data_out, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(J_DCT_METHOD, dct_method, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, do_fancy_upsampling, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, do_block_smoothing, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, quantize_colors, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(J_DITHER_MODE, dither_mode, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, two_pass_quantize, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, desired_number_of_colors, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, enable_1pass_quant, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, enable_external_quant, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, enable_2pass_quant, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, output_width, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, output_height, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, out_color_components, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, output_components, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, rec_outbuf_height, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, actual_number_of_colors, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JSAMPROW *, colormap, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, output_scanline, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, input_scan_number, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, input_iMCU_row, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, output_scan_number, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, output_iMCU_row, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int (*)[64], coef_bits, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JQUANT_TBL *[4], quant_tbl_ptrs, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JHUFF_TBL *[4], dc_huff_tbl_ptrs, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JHUFF_TBL *[4], ac_huff_tbl_ptrs, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, data_precision, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(jpeg_component_info *, comp_info, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, progressive_mode, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, arith_code, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(UINT8 [16], arith_dc_L, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(UINT8 [16], arith_dc_U, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(UINT8 [16], arith_ac_K, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, restart_interval, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, saw_JFIF_marker, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, JFIF_major_version, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, JFIF_minor_version, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, density_unit, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, X_density, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned short, Y_density, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, saw_Adobe_marker, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned char, Adobe_transform, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, CCIR601_sampling, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_marker_struct *, marker_list, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, max_h_samp_factor, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, max_v_samp_factor, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, min_DCT_scaled_size, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, total_iMCU_rows, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(JSAMPLE *, sample_range_limit, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, comps_in_scan, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(jpeg_component_info *[4], cur_comp_info, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, MCUs_per_row, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(unsigned int, MCU_rows_in_scan, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, blocks_in_MCU, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int [10], MCU_membership, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, Ss, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, Se, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, Ah, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, Al, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(int, unread_marker, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_decomp_master *, master, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_d_main_controller *, main, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_d_coef_controller *, coef, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_d_post_controller *, post, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_input_controller *, inputctl, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_marker_reader *, marker, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_entropy_decoder *, entropy, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_inverse_dct *, idct, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_upsampler *, upsample, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_color_deconverter *, cconvert, FIELD_NORMAL, ##__VA_ARGS__) \
	g() \
	f(struct jpeg_color_quantizer *, cquantize, FIELD_NORMAL, ##__VA_ARGS__) \
	g()
