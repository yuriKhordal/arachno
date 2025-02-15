#define AR_ERROR_NULL -1
#define AR_ERROR_ALLOC -2

#define AR_CHECK_NO_RETURN

#define CHECK_NULL(param, return_value) { \
	if (param == NULL) { \
		logf_error("%s: Argument "#param" was null.\n", __func__); \
		log_line(); \
		return return_value; \
	} \
}

#define CHECK_INDEX(param, max_value, return_value) { \
	if (param < 0 || param > (max_value)) { \
		logf_error("%s: Index "#param"(=%d) is out of range(length: %d).\n", \
			__func__, param, (max_value)); \
		log_line(); \
		return return_value; \
	} \
}

#define CHECK_RANGE(param, min_value, max_value, return_value) { \
	if (param < 0 || param > (max_value)) { \
		logf_error("%s: Argument "#param"(=%d) is out of range(%d:%d).\n", \
			__func__, param, (min_value), (max_value)); \
		log_line(); \
		return return_value; \
	} \
}
