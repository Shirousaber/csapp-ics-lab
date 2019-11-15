#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>

enum {
  NOTYPE = 256, ADD = '+', SUB = '-', MUL = '*' ,DEC_NUM = 10,
  DIV = '/', HEX_NUM = 16 , NEQ = 11 ,EQ = 1, OP = '(',CP = ')',
  REG_NAME = 255, NEG =2,DER = 3


};

static struct rule {
  char *regex;
  int token_type;
} rules[] = {

  /* Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", NOTYPE},    // spaces
  {"\\+", ADD},         // plus
  {"-",SUB},		//sub
  {"\\*",MUL},		//mul or indicator
  {"/", DIV},		//div	//decimal
  {"0[xX][0-9a-fA-F]+",HEX_NUM},
  {"[0-9]+",DEC_NUM},
  {"!=",NEQ},		//notequal
  {"==", EQ},         // equal
  {"\\(",OP},
  {"\\)",CP},
  {"\\$e(ax|bx|cx|dx|sp|bp|si|di|ip)",REG_NAME}
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

Token tokens[32];
int nr_token;
//Here are tow funcs i write to checkneg
//and to check dereference 
static bool checkneg(int pre_index)
{
  int pre_type =  tokens[pre_index].type;
  if(pre_type==SUB || pre_type == ADD||pre_type ==  MUL ||pre_type == DIV||pre_type == NEQ ||pre_type == EQ ||pre_type == OP)
  {
    return true;
  }
  return false;
}
//TODO:dereference_check
static bool checkdere(int pre_index)
{
  int pre_type = tokens[pre_index].type;
  if(pre_type==SUB || pre_type==ADD || pre_type == MUL ||pre_type == DIV ||pre_type == NEQ || pre_type == EQ || pre_type == OP)
  {
    return true;
  }
  return false;
}
//TODO:check_parantheses
static bool checkparantheses(int p, int q)
{
  int left_p_n = 0;
  int right_p_n = 0;
  if(tokens[p].type!='('||tokens[q].type!=')')
  {
    return false;//when there is no parantheses,return it
  }
  p++;
  for(int i=p; i<q;i++)
  {
   if(tokens[i].type=='(')
     left_p_n++;
   if(tokens[i].type==')')
     right_p_n++;
   if(right_p_n>left_p_n)
     return false;//match failure
  }
  if(right_p_n!=left_p_n)
    return false;
  return true;//shows that the parantheses matched
}
static int find_dominant_operator(int p, int q)
{
 //TODO
  int op = p;
  int quote_match = 0;
  for(int i=p;i<q;i++)
  {
    if(tokens[i].type == '(')
      quote_match++;  
    else if(tokens[i].type ==')')
    {
      quote_match--;
    }
    else if(quote_match == 0)
    {
      if(tokens[i].type==EQ || tokens[i].type==NEQ)
      {
        return i;
      }
     if(tokens[i].type == DER)
        continue;
     else if(tokens[i].type == ADD || tokens[i].type == SUB ||tokens[i].type == MUL || tokens[i].type == DIV)
     {
       if(tokens[i].type == ADD || tokens[i].type == SUB)
       {
       if(tokens[i-1].type==SUB)
        { return false;
        }
       op=i;
       }
       else{
       if(tokens[op].type== ADD || tokens[op].type== SUB);
       else op=i;

      }
     } 
    }
  }
  return op;
}
static int eval(int p, int q)//TODO:to calculate the exper vals
{
  if(p>q)
  {
    return false;
  }
  else if(p==q)
  {
    int number=0;
    if(tokens[p].type == HEX_NUM)
      sscanf(tokens[p].str,"%x",&number);
    else if(tokens[p].type == DEC_NUM)
      sscanf(tokens[p].str,"%d", &number);
    else if(tokens[p].type == REG_NAME)
    {
      for(int i=0;i<4;i++)
        tokens[p].str[i] = tokens[p].str[i+1];//
      if(strcmp(tokens[p].str,"eip")==0)
        number = cpu.eip;
      else{
      for(int i=0;i<8;i++)
      {  if(strcmp(tokens[p].str, regsl[i])==0)
        {
          number = cpu.gpr[i]._32;
          break;
        }  
      }
     }
   }
     return number; 
  }
  else 
  {
    int op = find_dominant_operator(p,q);
    //TODO 
    int l_eval = eval(p,op-1);
    int r_eval = eval(op+1,q);
    if(checkparantheses(p,q))//TODO:
    {
      return eval(p+1,q-1);
    }
    else if(tokens[op].type==DER) return vaddr_read(eval(op+1,q),4);
    else if(tokens[op].type==NEG) return -eval(op+1,q);
    int op_type = tokens[op].type;
    if(op_type==EQ) return l_eval==r_eval;
    else if(op_type==NEQ) return l_eval!=r_eval;
    else if(op_type==ADD) return l_eval+r_eval;
    else if(op_type==SUB) return l_eval-r_eval;
    else if(op_type==MUL) return l_eval*r_eval;
    else if(op_type==DIV) return l_eval/r_eval;
  }
return 0;
}
static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        if(nr_token>31){
            printf("The express is too long!\n");
	    return false;
	}
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);
        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

       switch (rules[i].token_type) {
          case NOTYPE:
	      tokens[nr_token].type = rules[i].token_type;
	      break;
	  case SUB://TODO:comple a func to judge withnin are negative
	      if(nr_token==0||checkneg(nr_token-1))
	      {
	          tokens[nr_token].type = NEG; 
	      }
              else
	      {
	          tokens[nr_token].type = rules[i].token_type;
	      }
	      break;
	  case MUL://TODO:dereference judge
	      if(nr_token==0||checkdere(nr_token-1))
	      {
	          tokens[nr_token].type = DER; 
	      }
              else
              {
	          tokens[nr_token].type = rules[i].token_type;
              }
         break;
         case DIV:
             tokens[nr_token].type = rules[i].token_type; 
         break;
         case DEC_NUM:
	 case HEX_NUM:
         case REG_NAME:
            if(substr_len > 31)           
               {
                printf("value too long!\n");
                return false;
               }
            strncpy(tokens[nr_token].str,substr_start,substr_len);
            tokens[nr_token].str[substr_len] = '\0';
            tokens[nr_token].type = rules[i].token_type; 
            break;
         default: tokens[nr_token].type = rules[i].token_type;
         break;   
        }
	++nr_token;
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

uint32_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  
  int p=0, q=nr_token-1;
  return eval(p,q);
}
