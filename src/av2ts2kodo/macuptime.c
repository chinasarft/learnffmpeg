#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <stdlib.h>

int main(){

	struct timeval tm;
	size_t s = sizeof(tm);
	memset(&tm, 0, sizeof(tm));
	
	sysctlbyname("kern.boottime", &tm, &s, NULL, 0);
	printf("%d %d\n", s, sizeof(tm));
	printf("%d\n%d\n", tm.tv_sec, tm.tv_usec);
}
