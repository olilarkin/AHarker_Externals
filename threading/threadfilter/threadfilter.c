
/*
 *  threadfilter
 *
 *	threadfilter filters its input into two streams, those in the low priority thread, and those in the high priority scheduler thread.
 *
 *	The most obvious use of threadfilter is to avoid threading issues, especially in the case of global code.
 *
 *  Copyright 2010 Alex Harker. All rights reserved.
 *
 */


#include <ext.h>
#include <ext_obex.h>


void *this_class;


typedef struct threadfilter{
    
	t_object a_obj;
	
	void *message_out_lo;
	void *message_out_hi;
	
} t_threadfilter;


t_symbol *ps_int;
t_symbol *ps_float;
t_symbol *ps_bang;


void *threadfilter_new();
void threadfilter_free(t_threadfilter *x);

void threadfilter_int(t_threadfilter *x, long intinput);
void threadfilter_float(t_threadfilter *x, double floatinput);
void threadfilter_bang(t_threadfilter *x);
void threadfilter_anything (t_threadfilter *x, t_symbol *msg, short argc, t_atom *argv);

void threadfilter_assist(t_threadfilter *x, void *b, long m, long a, char *s);


int main (void)
{	
	this_class = class_new("threadfilter", 
							(method) threadfilter_new, 
							(method) threadfilter_free, 
							sizeof(t_threadfilter), 
							NULL, 
							0);
	

	class_addmethod (this_class, (method)threadfilter_int, "int", A_LONG, 0);
	class_addmethod (this_class, (method)threadfilter_float, "float", A_FLOAT, 0);
	class_addmethod (this_class, (method)threadfilter_bang, "bang", 0);
	class_addmethod (this_class, (method)threadfilter_anything, "list", A_GIMME, 0);
	class_addmethod (this_class, (method)threadfilter_anything, "anything", A_GIMME, 0);
	class_addmethod (this_class, (method)threadfilter_assist, "assist", A_CANT, 0);
	
	class_register(CLASS_BOX, this_class);
	 
	ps_int = gensym("int");
	ps_float = gensym("float");
	ps_bang = gensym("bang");
	
	return 0;
}


void threadfilter_free(t_threadfilter *x)
{
}


void *threadfilter_new ()
{
    t_threadfilter *x = (t_threadfilter *) object_alloc (this_class);
	
	x->message_out_hi = outlet_new(x, 0);
	x->message_out_lo = outlet_new(x, 0);
	
    return (x);
}


void threadfilter_int(t_threadfilter *x, long intinput)
{
	if (isr())
		outlet_int(x->message_out_hi, intinput); 
	else
		outlet_int(x->message_out_lo, intinput); 
}


void threadfilter_float(t_threadfilter *x, double floatinput)
{
	if (isr())
		outlet_float(x->message_out_hi, floatinput); 
	else
		outlet_float(x->message_out_lo, floatinput); 
}


void threadfilter_bang(t_threadfilter *x)
{
	if (isr())
		outlet_bang(x->message_out_hi);
	else
		outlet_bang(x->message_out_lo); 
}


void threadfilter_anything (t_threadfilter *x, t_symbol *msg, short argc, t_atom *argv)
{
	if (isr())
		outlet_anything (x->message_out_hi, msg, argc, argv);
	else
		outlet_anything (x->message_out_lo, msg, argc, argv);
}


void threadfilter_assist(t_threadfilter *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_OUTLET)
    {
		switch (a)
		{
			case 0:
				sprintf(s, "Low Priority Messages");
				break;
		
			case 1:
				sprintf(s, "High Priority Messages");
				break;
		}
	}
	else
	{
		sprintf(s, "Input (anything)");
	}
}
	
	