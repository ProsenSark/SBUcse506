#include <inc/lib.h>
#include <inc/pincl.h>

int flag[256];

void lsdir(const char*, const char*);
void ls1(const char*, bool, off_t, const char*);

void
ls(const char *path, const char *prefix)
{
	int r;
	struct Stat st;

	if ((r = stat(path, &st)) < 0)
	{
		clog("stat %s: %e", path, r);
		exit();
	}
	if (st.st_isdir && !flag['d'])
		lsdir(path, prefix);
	else
		ls1(0, st.st_isdir, st.st_size, path);
}

void
lsdir(const char *path, const char *prefix)
{
	int fd, n;
	struct File f;

	//clog("wp1");
	if ((fd = open(path, O_RDONLY)) < 0)
	{
		clog("open %s: %e", path, fd);
		exit();
	}
	while ((n = readn(fd, &f, sizeof f)) == sizeof f)
	{
		if (f.f_name[0])
			ls1(prefix, f.f_type==FTYPE_DIR, f.f_size, f.f_name);
	}
	if (n > 0)
	{
		clog("short read in directory %s", path);
		exit();
	}
	if (n < 0)
	{
		clog("error reading directory %s: %e", path, n);
		exit();
	}
}

void
ls1(const char *prefix, bool isdir, off_t size, const char *name)
{
	char *sep;

	//clog("wp1");
	if(flag['l'])
		printf("%11d %c ", size, isdir ? 'd' : '-');
	if(prefix) {
		if (prefix[0] && prefix[strlen(prefix)-1] != '/')
			sep = "/";
		else
			sep = "";
		printf("%s%s", prefix, sep);
	}
	printf("%s", name);
	if(flag['F'] && isdir)
		printf("/");
	printf("\n");
}

void
usage(void)
{
	printf("usage: ls [-dFl] [file...]\n");
	exit();
}

void
umain(int argc, char **argv)
{
	int i;

	ARGBEGIN{
	default:
		usage();
	case 'd':
	case 'F':
	case 'l':
		flag[(uint8_t)ARGC()]++;
		break;
	}ARGEND

	if (argc == 0)
		ls("/", "");
	else {
		for (i=0; i<argc; i++)
			ls(argv[i], argv[i]);
	}
}

