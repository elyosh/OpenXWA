#include "xwa/util/string.h"

#include <ctype.h>

// GLOBAL: XWA 0x631860
char g_emptyString[1];

int Xwa_Stricmp(const char* left, const char* right) {
	unsigned char leftCh;
	unsigned char rightCh;

	do {
		leftCh = (unsigned char)tolower((unsigned char)*left++);
		rightCh = (unsigned char)tolower((unsigned char)*right++);
		if (leftCh != rightCh) {
			return (int)leftCh - (int)rightCh;
		}
	} while (leftCh != '\0');

	return 0;
}

// FUNCTION: XWA 0x4128D0
int StrCmpI(const char* lhs, const char* rhs) {
	const char* cursor;
	char ch;

	cursor = lhs;
	ch = *lhs;
	if (ch != '\0') {
		while (1) {
			int rhsLower;

			if (*rhs == '\0') {
				break;
			}
			rhsLower = tolower(*rhs);
			if (tolower(ch) != rhsLower) {
				int lhsLower;

				lhsLower = tolower(*cursor);
				return lhsLower <= tolower(*rhs) ? -1 : 1;
			}
			ch = *++cursor;
			++rhs;
			if (ch == '\0') {
				break;
			}
		}
	}

	if (*cursor != '\0' || *rhs != '\0') {
		return *cursor != '\0' ? 1 : -1;
	}
	return 0;
}
