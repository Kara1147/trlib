
#include <trlib.h>

int main(void)
{
	char src[] = "this is a tr test";

	tr(src, "tr", "TR", "");

	return 0;
}
