#include <inc/lib.h>
#include <inc/pincl.h>

int flag[256];
//char childpath[MAXPATHLEN];

void
rmfile(const char *path)
{
	int fd, r, n;
	struct Stat st;
	struct File f;
	char *childpath;

	//clog("wp1: %s", path);
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		clog("open %s: %e", path, fd);
		exit();
	}
	r = fstat(fd, &st);
	if (r < 0) {
		clog("stat %s: %e", path, r);
		exit();
	}

	if (st.st_isdir) {
		childpath = (char *) malloc(MAXPATHLEN * sizeof(char));
		if (childpath == NULL) {
			clog("malloc failed!");
			exit();
		}

		while ((n = readn(fd, &f, sizeof f)) == sizeof f) {
			if (f.f_name[0]) {
				if (flag['r']) {
					n = strlen(path);
					strncpy(childpath, path, n);
					childpath[n] = '/';
					strcpy(childpath + n + 1, f.f_name);
					//clog("wp2: %s", childpath);
					rmfile(childpath);
				}
				else {
					clog("rm %s: Directory not empty", path);
					exit();
				}
			}
		}

		free(childpath);
	}

	close(fd);
	remove(path);
}

void
usage(void)
{
	printf("usage: rm [-r] [file/directory...]\n");
	exit();
}

void
umain(int argc, char **argv)
{
	int i;

	ARGBEGIN{
	case 'r':
		flag[(uint8_t)ARGC()]++;
		break;
	default:
		usage();
	}ARGEND

	if (argc == 0)
		usage();
	else {
		for (i=0; i<argc; i++)
			rmfile(argv[i]);
	}
}

