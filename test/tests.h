#include <math.h>

typedef long (*plfv)(void);
typedef long (*plfl)(long);
typedef long (*plfll)(long, long);
typedef double (*pdfv)(void);

#define SUCCESS(x) printf("%s:\ttest%i\thas succeeded\n", __FILE__, x)
#define FAIL(x) printf("%s:\ttest%i\thas failed\n", __FILE__, x)
#define FAILX(x, r, expt) printf("%s:\ttest%i\thas failed. Return value is `%li' but expected `%li':%i\n", __FILE__, x, r, expt, r== expt)
#define FAILD(x, r, expt) printf("%s:\ttest%i\thas failed. Return value is `%f' but expected `%f':%i\n", __FILE__, x, r, expt, r == expt)

static inline int equal(double x, double y, double tolerance)
{
	return fabs(x - y) < tolerance;
}
