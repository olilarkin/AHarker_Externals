
/*
 *  timeconvolve~
 *
 *	timeconvolve~ copies samples from a buffer to use as a impulse response for real-time zero latency time-based convolution.
 *	
 *	Typically timeconvolve~ is suitable for use in conjunction with partconvolve~ for zero-latency convolution with longer impulses (timeconvolve use apple's vDSP and the IR length is limited to 2044 samples).
 *	The two objects have similar attributes / arguments and can be easily combined to design custom partitioning schemes.
 *	Note that in fact the algorithms perform correlation with reversed impulse response coeffients - which is equivalent to convolution.
 *
 *  Copyright 2010 Alex Harker. All rights reserved.
 *
 */


#include <ext.h>
#include <ext_obex.h>
#include <z_dsp.h>

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#endif

#include <AH_VectorOps.h>
#include <AH_Denormals.h>
#include <ibuffer_access.h>


AH_SIntPtr pad_length(AH_SIntPtr length)
{
	return ((length + 15) >> 4) << 4;
}


void *this_class;


typedef struct _timeconvolve
{
    t_pxobject x_obj;
	void *obex;
	
	// Buffer variables
	
	void *buffer_pointer;
	t_symbol *buffer_name;
	
	// Internal buffers
	
	float *impulse_buffer;
	float *input_buffer;
	float *output_buffer;
	
	long input_position;
	long impulse_length;
	
	// Attributes
	
	t_atom_long chan;
	t_atom_long offset;
	t_atom_long length;
	
	char memory_flag;
	
} t_timeconvolve;


void timeconvolve_free(t_timeconvolve *x);
void *timeconvolve_new(t_symbol *s, long argc, t_atom *argv);

void timeconvolve_set(t_timeconvolve *x, t_symbol *msg, long argc, t_atom *argv);

void time_domain_convolve_scalar(float *in, float *impulse, float *output, long N, long L);
void time_domain_convolve(float *in, vFloat *impulse, float *output, long N, long L);

void timeconvolve_perform_scalar_internal(t_timeconvolve *x, float *in, float *out, long vec_size);
t_int *timeconvolve_perform_scalar(t_int *w);
void timeconvolve_perform_internal(t_timeconvolve *x, float *in, float *out, long vec_size);
t_int *timeconvolve_perform(t_int *w);
void timeconvolve_dsp(t_timeconvolve *x, t_signal **sp, short *count);

void timeconvolve_perform_scalar64 (t_timeconvolve *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long vec_size, long flags, void *userparam);
void timeconvolve_perform64 (t_timeconvolve *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long vec_size, long flags, void *userparam);
void timeconvolve_dsp64 (t_timeconvolve *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);

void timeconvolve_assist(t_timeconvolve *x, void *b, long m, long a, char *s);


int C74_EXPORT main(void)
{
	this_class = class_new("timeconvolve~",
						   (method)timeconvolve_new, 
						   (method)timeconvolve_free, 
						   sizeof(t_timeconvolve), 
						   NULL, 
						   A_GIMME,
						   0);
	
	class_addmethod(this_class, (method)timeconvolve_set, "set", A_GIMME, 0);
	
    class_addmethod(this_class, (method)timeconvolve_assist, "assist", A_CANT, 0);
	class_addmethod(this_class, (method)timeconvolve_dsp, "dsp", A_CANT, 0);
	class_addmethod(this_class, (method)timeconvolve_dsp64, "dsp64", A_CANT, 0);
	
	class_addmethod(this_class, (method)object_obex_quickref, "quickref", A_CANT, 0);
	
	// Add Attributes
			
    CLASS_ATTR_LONG(this_class, "length", 0L, t_timeconvolve, length);
    CLASS_ATTR_FILTER_CLIP(this_class, "length", 0, 2044);
    CLASS_ATTR_LABEL(this_class, "length", 0L, "Impulse Length");
    
    CLASS_ATTR_LONG(this_class, "offset", 0L, t_timeconvolve, offset);
    CLASS_ATTR_FILTER_MIN(this_class, "offset", 0);
    CLASS_ATTR_LABEL(this_class,"offset", 0L, "Offset Into Buffer");
	
    CLASS_ATTR_LONG(this_class, "chan", 0L, t_timeconvolve, chan);
    CLASS_ATTR_FILTER_CLIP(this_class, "chan", 1, 4);
    CLASS_ATTR_LABEL(this_class, "chan", 0L, "Buffer Read Channel");

	// Add dsp and register 
	
	class_dspinit(this_class);
	class_register(CLASS_BOX, this_class);
	
	ibuffer_init();
	
	return 0;
}


void timeconvolve_free(t_timeconvolve *x)
{
	dsp_free(&x->x_obj);
	ALIGNED_FREE (x->impulse_buffer);
	ALIGNED_FREE (x->input_buffer);
}


void *timeconvolve_new(t_symbol *s, long argc, t_atom *argv)
{
	// Setup the object and make inlets / outlets
	
    t_timeconvolve *x = (t_timeconvolve *) object_alloc(this_class);
    
	long i;

    dsp_setup((t_pxobject *)x, 1);
    outlet_new((t_object *)x,"signal");
	
	// Set default initial attributes and variables
	
	x->buffer_pointer = 0;
	x->buffer_name = 0;
	
	x->offset = 0;
	x->length = 0;
	x->chan = 1;
	
	x->input_position = 0;
	x->impulse_length = 0;
	
	// Set attributes from arguments
	
	attr_args_process (x, argc, argv);
	
	// Allocate impulse buffer and input buffer
	
	x->impulse_buffer = ALIGNED_MALLOC (sizeof(float) * 2048);
	x->input_buffer = ALIGNED_MALLOC (sizeof(float) *  8192);
	
	for (i = 0; i < 2048; i++)
		x->impulse_buffer[i] = 0.f;

	for (i = 0; i < 8192; i++)
		x->input_buffer[i] = 0.f;

	x->memory_flag = (x->impulse_buffer && x->input_buffer);
	
	if (!x->memory_flag)
		object_error ((t_object *) x, "couldn't allocate enough memory.....");
	
	return (x);
}


void timeconvolve_set(t_timeconvolve *x, t_symbol *msg, long argc, t_atom *argv)
{
	// Standard ibuffer variables
	
	t_symbol *buffer_name = argc ? atom_getsym(argv) : 0; 
	void *b = ibuffer_get_ptr(buffer_name);
	void *buffer_samples_ptr;
	long n_chans;
	long format;
	
	// Attributes
	
	t_atom_long chan = x->chan - 1;
	t_atom_long offset = x->offset;
	t_atom_long length = x->length;
	
	float *impulse_buffer = x->impulse_buffer;
    
	AH_SIntPtr impulse_length;
#ifndef __APPLE__
	AH_SIntPtr impulse_offset, i;
#endif
	
	if (!x->memory_flag)
		return;
	
	if (b)
	{
		x->buffer_pointer = b;
		x->buffer_name = buffer_name;
		
		if (!ibuffer_info (b, &buffer_samples_ptr, &impulse_length, &n_chans, &format))
		{
			x->impulse_length = 0;
			return;
		}
		
		ibuffer_increment_inuse(b);
		
		if (n_chans < chan + 1)
			chan = chan % n_chans;
		
		// Calculate impulse length
		
		impulse_length -= offset;
		if (length && length < impulse_length)
			impulse_length = length;
		if (length && impulse_length < length)
			object_error ((t_object *) x, "buffer is shorter than requested length (after offset has been applied)");
		
		if (impulse_length < 0)
			impulse_length = 0;
		if (impulse_length > 2044)
			impulse_length = 2044;
        
        if (impulse_length)
        {
#ifdef __APPLE__
            ibuffer_get_samps_rev (buffer_samples_ptr, impulse_buffer, offset, impulse_length, n_chans, x->chan - 1, format);
#else
            impulse_offset = pad_length(impulse_length) - impulse_length;

            for (i = 0; i < impulse_offset; i++)
                 impulse_buffer[i] = 0.f;

            ibuffer_get_samps_rev (buffer_samples_ptr, impulse_buffer + impulse_offset, offset, impulse_length, n_chans, x->chan - 1, format);		
#endif
        }
		
        x->impulse_length = (long) impulse_length;

		ibuffer_decrement_inuse (b);
	}
	else
	{
		if (buffer_name)
		{
			object_error ((t_object *) x, "%s is not a valid buffer", buffer_name->s_name);
			x->buffer_pointer = 0;
			x->buffer_name = buffer_name;
			x->impulse_length = 0;
		}
		else 
		{
			x->buffer_pointer = 0;
			x->buffer_name = 0;
			x->impulse_length = 0;
		}
	}
}


#ifndef __APPLE__
void time_domain_convolve_scalar(float *in, float *impulse, float *output, long N, long L)
{
    
    float output_accum;
    float *input;
    
    long i, j;
    
    L = pad_length(L);
    
    for (i = 0; i < N; i++)
    {
        output_accum = 0.f;
        input = in - L + 1 + i;
        
        for (j = 0; j < L; j += 8)
        {
            // Load vals
            
            output_accum += impulse[j+0] * *input++;
            output_accum += impulse[j+1] * *input++;
            output_accum += impulse[j+2] * *input++;
            output_accum += impulse[j+3] * *input++;
            output_accum += impulse[j+4] * *input++;
            output_accum += impulse[j+5] * *input++;
            output_accum += impulse[j+6] * *input++;
            output_accum += impulse[j+7] * *input++;
        }
        
        *output++ = output_accum;
    }
}


void time_domain_convolve(float *in, vFloat *impulse, float *output, long N, long L)
{
	vFloat output_accum;
	float *input;
	float results[4];

	long i, j;
		
	L = pad_length(L);
				   
	for (i = 0; i < N; i++)
	{
		output_accum = float2vector(0.f);
		input = in - L + 1 + i;
		
		for (j = 0; j < L >> 2; j += 4)
		{
			// Load vals
			
			output_accum = F32_VEC_ADD_OP(output_accum, F32_VEC_MUL_OP(impulse[j], F32_VEC_ULOAD(input)));
			input += 4;
			output_accum = F32_VEC_ADD_OP(output_accum, F32_VEC_MUL_OP(impulse[j + 1], F32_VEC_ULOAD(input)));
			input += 4;
			output_accum = F32_VEC_ADD_OP(output_accum, F32_VEC_MUL_OP(impulse[j + 2], F32_VEC_ULOAD(input)));
			input += 4;
			output_accum = F32_VEC_ADD_OP(output_accum, F32_VEC_MUL_OP(impulse[j + 3], F32_VEC_ULOAD(input)));
			input += 4;
		}
		
		F32_VEC_USTORE(results, output_accum);
		
		*output++ = results[0] + results[1] + results[2] + results[3];
	}
}


void timeconvolve_perform_scalar_internal(t_timeconvolve *x, float *in, float *out, long vec_size)
{
	float *impulse_buffer = x->impulse_buffer;
	float *input_buffer = x->input_buffer;
	long input_position = x->input_position;
	long impulse_length = x->impulse_length;
	
	// Copy input twice (allows us to read input out in one go)
	
	memcpy(input_buffer + input_position, in, sizeof(float) * vec_size);
	memcpy(input_buffer + 4096 + input_position, in, sizeof(float) * vec_size);
	
	// Advance pointer 
	
	input_position += vec_size;
	if (input_position >= 4096) 
		input_position = 0;
	x->input_position = input_position;
	
	// Do convolution
	
	time_domain_convolve_scalar(input_buffer + 4096 + (input_position - vec_size), impulse_buffer, out, vec_size, impulse_length);
}


t_int *timeconvolve_perform_scalar(t_int *w)
{
    // Miss perform routine for denormal handling (w[2] onwards)

    timeconvolve_perform_scalar_internal((t_timeconvolve *)(w[5]), (float *)(w[2]), (float *)(w[3]), (long) (w[4]));
    
    return w + 6;
}
#endif


void timeconvolve_perform_internal(t_timeconvolve *x, float *in, float *out, long vec_size)
{
	float *impulse_buffer = x->impulse_buffer;
	float *input_buffer = x->input_buffer;
	long input_position = x->input_position;
	long impulse_length = x->impulse_length;
	
	// Copy input twice (allows us to read input out in one go)
	
	memcpy(input_buffer + input_position, in, sizeof(float) * vec_size);
	memcpy(input_buffer + 4096 + input_position, in, sizeof(float) * vec_size);
	
	// Advance pointer 
	
	input_position += vec_size;
	if (input_position >= 4096) 
		input_position = 0;
	x->input_position = input_position;
	
	// Do convolution
	
#ifdef __APPLE__
	vDSP_conv(input_buffer + 4096 + input_position - (impulse_length + vec_size) + 1, 1, impulse_buffer, 1, out, 1, vec_size, impulse_length);
#else
	time_domain_convolve(input_buffer + 4096 + (input_position - vec_size), (vFloat *) impulse_buffer, out, vec_size, impulse_length);
#endif
}


t_int *timeconvolve_perform(t_int *w)
{
    // Miss perform routine for denormal handling (w[2] onwards)
    
    timeconvolve_perform_internal((t_timeconvolve *)(w[5]), (float *)(w[2]), (float *)(w[3]), (long) (w[4]));
    
    return w + 6;
}


void timeconvolve_dsp(t_timeconvolve *x, t_signal **sp, short *count)
{
#ifndef __APPLE__
	if (SSE2_check())
		dsp_add(denormals_perform, 5, timeconvolve_perform, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n, x);
	else
		dsp_add(denormals_perform, 5, timeconvolve_perform_scalar, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n, x);
#else
    dsp_add(denormals_perform, 5, timeconvolve_perform, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n, x);
#endif
}


#ifndef __APPLE__
void timeconvolve_perform_scalar64 (t_timeconvolve *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long vec_size, long flags, void *userparam)
{
	double *in = ins[0];
	double *out = outs[0];
	float *temp_in = (float *) outs[0];
	float *temp_out = temp_in + vec_size;
		
	// Copy in
	
    for (long i = 0; i < vec_size; i++)
        temp_in[i] = *in++;
	
	// Process
	
    timeconvolve_perform_scalar_internal(x, temp_in, temp_out, vec_size);
	
	// Copy out
	
	for (long i = 0; i < vec_size; i++)
		*out++ = (double) temp_out[i];
}
#endif


void timeconvolve_perform64 (t_timeconvolve *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long vec_size, long flags, void *userparam)
{
	double *in = ins[0];
	double *out = outs[0];
	float *temp_in = (float *) outs[0];
	float *temp_out = temp_in + vec_size;
		
	// Copy in
	
    for (long i = 0; i < vec_size; i++)
        temp_in[i] = *in++;
	
	// Process
	
    timeconvolve_perform_internal(x, temp_in, temp_out, vec_size);
	
	// Copy out
	
	for (long i = 0; i < vec_size; i++)
		*out++ = (double) temp_out[i];
}


void timeconvolve_dsp64 (t_timeconvolve *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
#ifndef __APPLE__
	if (SSE2_check())
		object_method(dsp64, gensym("dsp_add64"), x, timeconvolve_perform64);
	else
		object_method(dsp64, gensym("dsp_add64"), x, timeconvolve_perform_scalar64);
#else
    object_method(dsp64, gensym("dsp_add64"), x, timeconvolve_perform64);
#endif
}


void timeconvolve_assist(t_timeconvolve *x, void *b, long m, long a, char *s)
{
    if (m == ASSIST_OUTLET)
		sprintf(s,"(signal) Convolved Output");
	else 
        sprintf(s,"(signal) Input");
}
