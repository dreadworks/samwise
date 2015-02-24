
endpoint = "ipc://../../sam_ipc"


RSpec.describe Samwise::RabbitMQ do

  before(:all) do
    Samwise::Connection.connect endpoint
  end

  after(:all) do
    Samwise::Connection.close!
  end


  it "handles round robin" do
    Samwise::RabbitMQ.publish_roundrobin "amq.direct", "", "hi!"
  end


  it "handles redundant" do
    Samwise::RabbitMQ.publish_redundant 2, "amq.direct", "", "hi!"
  end


  it "handles exchange.declare" do
    Samwise::RabbitMQ.exchange_declare "test-x", "direct"
  end


  it "handles exchange.delete" do
    Samwise::RabbitMQ.exchange_delete "test-x"
  end


end
