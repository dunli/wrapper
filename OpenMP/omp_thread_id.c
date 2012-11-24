#include <omp.h>

/*获取OMP线程号*/
int get_thread_num (void)
{
	return omp_get_thread_num();
}

int get_level ()
{
	return omp_get_level ();
}
