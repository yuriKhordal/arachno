#define ARC_ERR_NULL -1
#define ARC_ERR_ALLOC -2

#define ARC_CHECK_NULL(param, return_value) { \
	if (param == NULL) { \
		logf_error("%s: Argument "#param" was null.\n", __func__); \
		log_line(); \
		return return_value; \
	} \
}

#define ARC_CHECK_INDEX(param, max_value, return_value) { \
	if (param < 0 || param > (max_value)) { \
		logf_error("%s: Index "#param"(=%d) is out of range(length: %d).\n", \
			__func__, param, (max_value)); \
		log_line(); \
		return return_value; \
	} \
}

#define ARC_CHECK_RANGE(param, min_value, max_value, return_value) { \
	if (param < 0 || param > (max_value)) { \
		logf_error("%s: Argument "#param"(=%d) is out of range(%d:%d).\n", \
			__func__, param, (min_value), (max_value)); \
		log_line(); \
		return return_value; \
	} \
}

#if defined(ARC_NO_PREFIX) || defined(ARC_CHECK_NO_PREFIX)
	#define CHECK_NULL(param, return_value) ARC_CHECK_NULL(param, return_value)
	#define CHECK_INDEX(param, max_value, return_value) ARC_CHECK_INDEX(param, max_value, return_value)
	#define CHECK_RANGE(param, min_value, max_value, return_value) ARC_CHECK_RANGE(param, min_value, max_value, return_value)
#endif
