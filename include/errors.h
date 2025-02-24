// Represents an error caused by a null value or argument.
#define ARC_ERR_NULL -10
// Represents an error caused by a failed allocation.
#define ARC_ERR_ALLOC -11
// Represents an error related to sockets. Opening, reading, writing, sending, etc'.
#define ARC_ERR_SOCK -12
// Represents an error in syntax.
#define ARC_ERR_SYNTAX -13
#define ARC_ERR_NO_LENGTH -14
#define ARC_ERR_UNSUPPORTED_HTTP_VER -48
#define ARC_ERR_UNSUPPORTED -49
#define ARC_ERR_OTHER -50
#define ARC_ERR_NOT_IMPLEMENTED -51

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
