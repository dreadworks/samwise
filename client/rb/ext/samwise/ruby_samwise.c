#include <ruby.h>
#include "samwise.h"

VALUE samwise;

void init_samwise ()
{
    samwise = rb_define_module ("Samwise");
    VALUE cSamwiseRabbitMQ = rb_define_class_under (samwise, "RabbitMQ", rb_cObject);
}
