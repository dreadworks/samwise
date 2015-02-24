

# Samwise Ruby Gem #

This is a ruby implementation of the samwise protocol 1.0 to communicate with samwise instances.

## Examples ##

```ruby

Samwise::Connection.connect "ipc://path/to/samwise"

Samwise::RabbitMQ.publish_roundrobin "amq.direct", "", "hi!"

Samwise::RabbitMQ.publish_redundant 2, "amq.direct", "", "hi!"

Samwise::RabbitMQ.exchange_declare "test-x"

Samwise::RabbitMQ.exchange_delete "test-x"

Samwise::Connection.close!

```
