#include <inc/lib.h>
#include <inc/pincl.h>

int flag[256];

// Skip over slashes.
static const char*
skip_slash(const char *p)
{
	while (*p == '/') {
		p++;
	}
	return p;
}

void
mkdir(const char *path)
{
	const char *p;
	char name[MAXNAMELEN];
	int fd;

	//clog("wp1");
	p = path;
	p = skip_slash(p);
	if (flag['p']) {
		while (*p != '\0') {
			while (*p != '/' && *p != '\0') {
				p++;
			}

			memmove(name, path, p - path);
			name[p - path] = '\0';
			fd = open(name, O_RDONLY);
			if (fd < 0) {
				if (fd == -E_NOT_FOUND) {
					fd = open(name, O_WRONLY | O_CREAT | O_EXCL | O_MKDIR);
					if (fd < 0) {
						clog("open %s: %e", name, fd);
						exit();
					}
					close(fd);
				}
				else {
					clog("open %s: %e", name, fd);
					exit();
				}
			}
			close(fd);

			p = skip_slash(p);
		}
	}
	else {
		fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_MKDIR);
		if (fd < 0) {
			clog("open %s: %e", path, fd);
			exit();
		}
		close(fd);
	}
}

void
usage(void)
{
	printf("usage: mkdir [-p] [directory...]\n");
	exit();
}

void
umain(int argc, char **argv)
{
	int i;

	ARGBEGIN{
	case 'p':
		flag[(uint8_t)ARGC()]++;
		break;
	default:
		usage();
	}ARGEND

	if (argc == 0)
		usage();
	else {
		for (i=0; i<argc; i++)
			mkdir(argv[i]);
	}
}

