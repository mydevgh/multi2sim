#include <stdio.h>

struct si_stream_t
{
	FILE *out_file;
	long offset;
};

struct si_stream_t *si_stream_create(char *fileName);
void si_stream_add_inst(struct si_stream_t *stream, struct si_inst_t *inst);
long si_stream_get_offset(struct si_stream_t *stream);
void si_stream_resolve_task(struct si_stream_t *stream, struct si_task_t *task);
void si_stream_close(struct si_stream_t *stream);