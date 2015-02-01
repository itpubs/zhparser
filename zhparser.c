/*-------------------------------------------------------------------------
 *
 * zhparser.c
 *	  a text search parser for Chinese
 *
 *-------------------------------------------------------------------------
 */
#include "zhparser.h"

#include "postgres.h"
#include "miscadmin.h"
#include "fmgr.h"


PG_MODULE_MAGIC;

#define index(c) ((unsigned int)(c) - (unsigned int)'a')
/*
 * types
 */

/* self-defined type */
typedef struct
{
	char	   *buffer;			/* text to parse */
	int		len;			/* length of the text in buffer */
	int		pos;			/* position of the parser */
	scws_t scws;
	scws_res_t head;
	scws_res_t curr;
	char * table;
} ParserState;

/* copy-paste from wparser.h of tsearch2 */
typedef struct
{
	int			lexid;
	char	   *alias;
	char	   *descr;
} LexDescr;

static void init();

static void init_type(LexDescr descr[]);

/*
 * prototypes
 */
PG_FUNCTION_INFO_V1(zhprs_start);
Datum		zhprs_start(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(zhprs_getlexeme);
Datum		zhprs_getlexeme(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(zhprs_end);
Datum		zhprs_end(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(zhprs_lextype);
Datum		zhprs_lextype(PG_FUNCTION_ARGS);

static scws_t scws = NULL;
static char a[26]={'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};
static int type_inited = 0;

static ParserState parser_state;

static void init(){
	char sharepath[MAXPGPATH];
	char dict_path[MAXPGPATH];
	char rule_path[MAXPGPATH];

	if (!(scws = scws_new())) {
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Chinese Parser Lib SCWS could not init!\"%s\"",""
				       )));
	}
	get_share_path(my_exec_path, sharepath);

	snprintf(dict_path, MAXPGPATH, "%s/tsearch_data/%s.%s",
			sharepath, "dict.utf8", "xdb");
	scws_set_charset(scws, "utf-8");
	scws_set_dict(scws,dict_path, SCWS_XDICT_XDB);

	snprintf(rule_path, MAXPGPATH, "%s/tsearch_data/%s.%s",
			sharepath, "rules.utf8", "ini");
	scws_set_rule(scws ,rule_path);
}

/*
 * functions
 */
Datum
zhprs_start(PG_FUNCTION_ARGS)
{
	ParserState *pst = &parser_state;
	if(scws == NULL)
		init();
	pst -> scws = scws;
	pst->buffer = (char *) PG_GETARG_POINTER(0);
	pst->len = PG_GETARG_INT32(1);
	pst->pos = 0;

	scws_send_text(pst -> scws, pst -> buffer, pst -> len);

	pst -> curr = (scws_res_t)-1;
	pst -> head = (scws_res_t)NULL;

	pst -> table = (char *)a;
	PG_RETURN_POINTER(pst);
}

Datum
zhprs_getlexeme(PG_FUNCTION_ARGS)
{
	ParserState *pst = (ParserState *) PG_GETARG_POINTER(0);
	char	  **t = (char **) PG_GETARG_POINTER(1);
	int		   *tlen = (int *) PG_GETARG_POINTER(2);
	int			type = -1;

	/* first time */
	if((pst -> curr) == (scws_res_t)-1 ){

		(pst -> head) = (pst -> curr) = scws_get_result(pst -> scws);
	}

	if((pst -> head) == NULL ) /* already done the work */
	{
		*tlen = 0;
		type = 0;
	}
	/* have results */
	else if(pst -> curr != NULL)
	{
		scws_res_t  curr = pst -> curr;

		/*
 		* check the first char to determine the lextype
 		* if out of [0,25],then set to 'x',mean unknown type 
 		* so for Ag,Dg,Ng,Tg,Vg,the type will be unknown
 		* for full attr explanation,visit http://www.xunsearch.com/scws/docs.php#attr
		*/
		unsigned int idx = index((curr -> attr)[0]);
		if(idx > 25)
			idx = (unsigned int)23;
		type = (int)((pst -> table)[idx]);
		*tlen = curr -> len;
		*t = pst -> buffer + curr -> off;

		pst -> curr = curr -> next;

		/* fetch next sentence */
		if(pst -> curr == NULL ){
			scws_free_result(pst -> head);
			(pst -> head) =	(pst -> curr) = scws_get_result(pst -> scws);
		}
	}

	PG_RETURN_INT32(type);
}

Datum
zhprs_end(PG_FUNCTION_ARGS)
{
	PG_RETURN_VOID();
}

Datum
zhprs_lextype(PG_FUNCTION_ARGS)
{
	static LexDescr   descr[27];
	if(type_inited == 0){
	    init_type(descr);
	    type_inited = 1;
	}

	PG_RETURN_POINTER(descr);
}

static void init_type(LexDescr descr[]){
	/* 
	* there are 26 types in this parser,alias from a to z
	* for full attr explanation,visit http://www.xunsearch.com/scws/docs.php#attr
	*/
	descr[0].lexid = 97;
	descr[0].alias = pstrdup("a");
	descr[0].descr = pstrdup("adjective");
	descr[1].lexid = 98;
	descr[1].alias = pstrdup("b");
	descr[1].descr = pstrdup("differentiation (qu bie)");
	descr[2].lexid = 99;
	descr[2].alias = pstrdup("c");
	descr[2].descr = pstrdup("conjunction");
	descr[3].lexid = 100;
	descr[3].alias = pstrdup("d");
	descr[3].descr = pstrdup("adverb");
	descr[4].lexid = 101;
	descr[4].alias = pstrdup("e");
	descr[4].descr = pstrdup("exclamation");
	descr[5].lexid = 102;
	descr[5].alias = pstrdup("f");
	descr[5].descr = pstrdup("position (fang wei)");
	descr[6].lexid = 103;
	descr[6].alias = pstrdup("g");
	descr[6].descr = pstrdup("root (ci gen)");
	descr[7].lexid = 104;
	descr[7].alias = pstrdup("h");
	descr[7].descr = pstrdup("head");
	descr[8].lexid = 105;
	descr[8].alias = pstrdup("i");
	descr[8].descr = pstrdup("idiom");
	descr[9].lexid = 106;
	descr[9].alias = pstrdup("j");
	descr[9].descr = pstrdup("abbreviation (jian lue)");
	descr[10].lexid = 107;
	descr[10].alias = pstrdup("k");
	descr[10].descr = pstrdup("head");
	descr[11].lexid = 108;
	descr[11].alias = pstrdup("l");
	descr[11].descr = pstrdup("tmp (lin shi)");
	descr[12].lexid = 109;
	descr[12].alias = pstrdup("m");
	descr[12].descr = pstrdup("numeral");
	descr[13].lexid = 110;
	descr[13].alias = pstrdup("n");
	descr[13].descr = pstrdup("noun");
	descr[14].lexid = 111;
	descr[14].alias = pstrdup("o");
	descr[14].descr = pstrdup("onomatopoeia");
	descr[15].lexid = 112;
	descr[15].alias = pstrdup("p");
	descr[15].descr = pstrdup("prepositional");
	descr[16].lexid = 113;
	descr[16].alias = pstrdup("q");
	descr[16].descr = pstrdup("quantity");
	descr[17].lexid = 114;
	descr[17].alias = pstrdup("r");
	descr[17].descr = pstrdup("pronoun");
	descr[18].lexid = 115;
	descr[18].alias = pstrdup("s");
	descr[18].descr = pstrdup("space");
	descr[19].lexid = 116;
	descr[19].alias = pstrdup("t");
	descr[19].descr = pstrdup("time");
	descr[20].lexid = 117;
	descr[20].alias = pstrdup("u");
	descr[20].descr = pstrdup("auxiliary");
	descr[21].lexid = 118;
	descr[21].alias = pstrdup("v");
	descr[21].descr = pstrdup("verb");
	descr[22].lexid = 119;
	descr[22].alias = pstrdup("w");
	descr[22].descr = pstrdup("punctuation (qi ta biao dian)");
	descr[23].lexid = 120;
	descr[23].alias = pstrdup("x");
	descr[23].descr = pstrdup("unknown");
	descr[24].lexid = 121;
	descr[24].alias = pstrdup("y");
	descr[24].descr = pstrdup("modal (yu qi)");
	descr[25].lexid = 122;
	descr[25].alias = pstrdup("z");
	descr[25].descr = pstrdup("status (zhuang tai)");
	descr[26].lexid = 0;
}
//TODO :headline function
