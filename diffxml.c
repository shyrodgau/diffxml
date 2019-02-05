/*
* diffxml
* produce xml from a diff output (only fragment, place surrounding tag yourself)
* It is very crude and missing a lot of basic error handling!!
* In doubt adjust the BUFSIZE upward.
* Currently only supports Unix-style text only. (i.e. only newline as line terminator)
*
* compile: gcc -Wall -o diffxml diffxml.c
*
* echo '<diffs>' > diff.xml
* diff -N -w -r -p dira/ dirb/ | diffxml >> diff.xml
* echo '</diffs>' >> diff.xml
* 
* for example linux kernels, breaking it down across the toplevel directory structure
* to make the chunks smaller and the output better structured:
*
* d1=linux-4.14.1
* d2=linux-4.14.2
* echo '<knldiffs>' > knldiffs.xml
* for f in $( for d in ${d1}/_* ${d2}/_* # remove 2x _
* do
*    echo ${d##*_/} # remove  _ 
* done |sort -u )
* do
*    echo '<blk n="'$f'">' >> knldiffs.xml
*    diff  -N -w -r -p ${d1}/$f ${d2}2/$f > /tmp/x; diffxml /tmp/x >> knldiffs.xml
*    echo '</blk>' >> knldiffs.xml
* done
* echo '</knldiffs>' >> knldiffs.xml
*
*/


#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>



#define BUFSIZE 12000000
#define AUXSIZE 256


char *buf;


typedef struct _dl {
	struct _dl *next;
	char *line;
} dl;

typedef struct _dlines {
	struct _dlines *next;
	char fname[AUXSIZE];
	unsigned int stleft;
	unsigned int stright;
	unsigned int nleft;
	unsigned int nright;
	dl *left;
	dl *right;
} dlines;

typedef struct _dfile {
	struct _dfile *next;
	char nleft[AUXSIZE];
	char nright[AUXSIZE];
	char restleft[AUXSIZE];
	char restright[AUXSIZE];
	unsigned int ndiffs;
	dlines	*diffs;
} dfile;

dfile *dfiles = NULL;

struct _filelines {
	unsigned int nlines;
	char **lstart;
} filelines;


static void printesc(const char *s)
{
	while (*s)
	{
		if ('<' == *s)
			printf("&lt;");
		else
			printf("%c",*s);
		s++;
	}
}

static void breakFile(unsigned int insize)
{
	char *f = buf;
	int i=0;
	filelines.nlines = 0;

	filelines.lstart[0] = buf;
	
	while (i<insize) 
	{
		char *of = f;
		f = memchr(of,0xa,insize-i);
		
		if (NULL == f)
		{
			break;
		}
		else
		{
			filelines.nlines++;
			filelines.lstart[filelines.nlines] = f + 1;
			*f = 0;
			i += ((f - of) + 1);
			f++;
		}
	}
}

static int parseDlines1(unsigned int l, dl **ls, unsigned int *pn, unsigned int *pst)
{
	unsigned int n=0;
	if ((memcmp(filelines.lstart[l],"*** ", 4)) && memcmp(filelines.lstart[l],"--- ",4))
	{
		fprintf(stderr,"Parsing at line %d unexpected, not *** linenos \'%s\'\n",l, filelines.lstart[l]);
		return 999999;
	}
	*pst = 0;
	char *c = filelines.lstart[l] + 4;
	while ((*c <= '9') && (*c >= '0'))
		*pst = (10 * *pst) + (*(c++)  - '0');
	if (',' == *c)
	{
		c++;
		n = 0;
		while ((*c <= '9') && (*c >= '0'))
			n = (10 * n) + (*(c++)  - '0');
		n -= (*pst - 1);
	}
	else if (*pst != 0)
	{
		//fprintf(stderr,"Parsing at line %d unexpected, no , though not 0 \'%s\'\n",l, filelines.lstart[l]);
		n=1;
	}
	else
		n=0;
	*pn = n;
	
	if ((l+1 >= filelines.nlines) || !memcmp(filelines.lstart[l+1],"---", 3) || (!memcmp(filelines.lstart[l+1],"*****",5)) || (!memcmp(filelines.lstart[l+1],"diff ",5)))
	{
		return 1;
	}
	else if (n)
	{
		*ls = malloc(sizeof(dl));
		memset(*ls, 0, sizeof(dl));
	}
	else
		return 1;

	l++;
	dl *myl = (*ls);
	while (n)
	{
		myl->line = filelines.lstart[l++];
		n--;
		if (n)
		{
			myl->next = malloc(sizeof(dl));
			myl = myl->next;
			memset(myl, 0, sizeof(dl));
		}
	}
	if (!memcmp(filelines.lstart[l],"\\ No newline", 12))
	{
                myl->next = malloc(sizeof(dl));
                myl = myl->next;
                memset(myl, 0, sizeof(dl));
		myl->line = filelines.lstart[l];
		//(*pn)++;
	}

	return (*pn + 1);
}

static int parseDlines(unsigned int l, dfile *dfi)
{
	unsigned int l0 = l;
	if (memcmp(filelines.lstart[l],"***************", strlen("***************")))
		return 0;
	
	dlines *dl;
	if (dfi->diffs)
	{
		dl = dfi->diffs;
		while (dl->next)
			dl = dl->next;
		dl->next = malloc(sizeof(dlines));
		dl = dl->next;
	}
	else
	{
		dl = malloc(sizeof(dlines));
		dfi->diffs = dl;
	}
	memset(dl,0,sizeof(dlines));
	if (!memcmp(filelines.lstart[l],"*************** ", strlen("*************** ")))
		strncpy(dl->fname, filelines.lstart[l] + 16, AUXSIZE - 1);

	l++;
	l += parseDlines1(l, &(dl->left), &(dl->nleft), &(dl->stleft));
	if ((' ' == *filelines.lstart[l+1]) || ('+' == *filelines.lstart[l+1]) || (!memcmp(filelines.lstart[l],"--- ",4)) ||
		('-' == *filelines.lstart[l+1]) || ('!' == *filelines.lstart[l+1]))
		l += parseDlines1(l, &(dl->right), &(dl->nright), &(dl->stright));
	else l+=1;

	return (l - l0);
}

static int parseDfile(unsigned int l, dfile *dfi)
{
	unsigned int l0 = l;
	if (memcmp(filelines.lstart[l],"*** ", 4))
	{
		fprintf(stderr,"Parsing at line %d unexpected, not *** \'%s\'\n",l, filelines.lstart[l]);
		return 999999;
	}
	if (memcmp(filelines.lstart[l+1],"--- ", 4))
	{
		fprintf(stderr,"Parsing at line %d unexpected, not --- \'%s\'\n",l+1, filelines.lstart[1+l]);
		return 999999;
	}
	char *nleft = filelines.lstart[l] + 4;
	char *enleft = filelines.lstart[l] + strlen(filelines.lstart[l]) - strlen("2019-01-18 23:36:05.829992635 +0100");  //strchr(nleft,' ');
	if (!enleft)
	{
		fprintf(stderr,"Parsing at line %d, no end of l filename \'%s\'\n",l, nleft);
		return 999999;
	}
	*(enleft - 1) = 0;
	strncpy(dfi->nleft, nleft, AUXSIZE - 1);
	strncpy(dfi->restleft, enleft, AUXSIZE - 1);

	char *nright = filelines.lstart[l+1] + 4;
	char *enright = filelines.lstart[l+1] + strlen(filelines.lstart[l+1]) - strlen("2019-01-18 23:36:05.829992635 +0100");  //strchr(nright,' ');
	if (!enright)
	{
		fprintf(stderr,"Parsing at line %d, no end of r filename \'%s\'\n",l+1, nright);
		return 999999;
	}
	*(enright - 1) = 0;
	strncpy(dfi->nright, nright, AUXSIZE - 1);
	strncpy(dfi->restright, enright, AUXSIZE - 1);

	l += 2;
	int dl=1;
	while ((l < filelines.nlines) && dl)
	{
		dl = parseDlines(l,dfi);
		l += dl;
	}
	return (l - l0);
}

static void parseFile()
{
	int l = 0;
	while (l < filelines.nlines)
	{
		if (memcmp(filelines.lstart[l],"diff ", 5))
		{
			if (l) fprintf(stderr,"Parsing at line %d unexpected, not diff: \'%s\'\n",l, filelines.lstart[l]);
			l--;
		}
//		else
		{
			l++;
			dfile *dfi;
			if (dfiles)
			{
				dfi = dfiles;
				while (dfi->next)
					dfi = dfi->next;
				dfi->next = malloc(sizeof(dfile));
				dfi = dfi->next;
			}
			else
			{
				dfiles = malloc(sizeof(dfile));
				dfi = dfiles;
			}
			memset(dfi, 0, sizeof(dfile));
			l += parseDfile(l, dfi);
		}
	}
}

void toxmldl(dlines *dli)
{
	if (strchr(dli->fname,'"') || ((!strchr(dli->fname,'"')) && (!strchr(dli->fname,'\''))))
	{
		printf("<di fname='");
		printesc(dli->fname);
		printf("'>\n");
	}
	else if (strchr(dli->fname,'\''))
	{
		printf("<di fname=\"");
		printesc(dli->fname);
		printf("\">\n");
	}
	else
	{
		printf("<di fname=\"...\">\n");
	}
	printf("<lines for=\"left\" start=\"%d\" n=\"%d\">", dli->stleft, dli->nleft);
	//printf("<lines xml:space='preserve' for=\"left\" start=\"%d\" n=\"%d\">", dli->stleft, dli->nleft);
	int first=0;
	if (dli->left)
	{
		if (!first)
			printf("\n");
		first=1;
		dl *l = dli->left;
		//printf("<![CDATA[\r\n");
		while (l)
		{
			//printf("%s\r\n",l->line);
			//printf("<l xml:space='preserve'><![CDATA[%s]]></l>",l->line);
			printf("<l xml:space='preserve'><![CDATA[%s]]></l>\n",l->line);
			l=l->next;
		}
		//printf("]]>");
	}
	printf("</lines>");
	first=0;
	printf("<lines for=\"right\" start=\"%d\" n=\"%d\">", dli->stright, dli->nright);
	//printf("<lines xml:space='preserve' for=\"right\" start=\"%d\" n=\"%d\">", dli->stright, dli->nright);
	if (dli->right)
	{
		if (!first)
			printf("\n");
		first=1;
		dl *l = dli->right;
		//printf("<![CDATA[\r\n");
		while (l)
		{
			//printf("%s\r\n",l->line);
			//printf("<l xml:space='preserve'><![CDATA[%s]]></l>",l->line);
			printf("<l xml:space='preserve'><![CDATA[%s]]></l>\n",l->line);
			l=l->next;
		}
		//printf("]]>");
	}
	printf("</lines>");

	printf("</di>");
	if (dli->next)
		toxmldl(dli->next);
}

void toxml(dfile *df)
{
	printf("<fi left=\"%s\" right=\"%s\" lrest=\"%s\" rrest=\"%s\">", df->nleft, df->nright, df->restleft, df->restright);

	if (df->diffs)
		toxmldl(df->diffs);

	printf("</fi>");
	if (df->next)
		toxml(df->next);
}

int main (int argc, char ** argv)
{
	struct stat mystat;
	if (argc < 2)
	{
		fprintf(stderr,"Usage: %s filename\n", argv[0]);
		exit(1);
	}
	if (stat(argv[1], &mystat))
	{
		perror("Failed to get length of input file");
		exit(2);
	}
	buf = malloc(mystat.st_size + 1);
	if (!buf)
	{
		perror("Failed to alloc buffer for input file");
		exit(3);
	}
	filelines.lstart = malloc(sizeof(char*) * mystat.st_size);
	if (!filelines.lstart)
	{
		perror("Failed to alloc buffer for input file lines");
		exit(4);
	}
	FILE *fh = fopen(argv[1],"r");
	if (!fh)
	{
		perror("Failed to open input file");
		exit(5);
	}
	
	int n = fread(buf, 1, mystat.st_size + 1, fh);
	fclose(fh);
	if (n < mystat.st_size)
	{
		fprintf(stderr,"Could only read %d B from input file, expected %d\n", n, (int)mystat.st_size);
		exit(6);
	}
	breakFile(n);
	parseFile();
	if (dfiles) toxml(dfiles);
	return 0;
}

