typedef long (*plfv)(void);
typedef long (*plfl)(long);
typedef long (*plfll)(long, long);

#define SUCCESS(x) printf("%s:\ttest%i\thas succeeded\n", __FILE__, x)
#define FAIL(x) printf("%s:\ttest%i\thas failed\n", __FILE__, x)
#define FAILX(x, r, expt) printf("%s:\ttest%i\thas failed. Return value is `%li' but expected `%li':%i\n", __FILE__, x, r, expt, r== expt)
