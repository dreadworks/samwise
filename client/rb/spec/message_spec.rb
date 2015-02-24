
endpoint = "ipc://../../sam_ipc"


RSpec.describe Samwise::Message do

  before(:all) do
    Samwise::Connection.connect endpoint
  end

  after(:all) do
    Samwise::Connection.close!
  end

  before(:each) do
    @msg = Samwise::Message.new
  end


  it "creates a proper message" do
    expect(@msg).to be_an_instance_of(Samwise::Message)
    expect(@msg.empty?).to be true
    expect(@msg.size).to eq 0
    expect { @msg.send }.to raise_error Samwise::RequestMalformed
  end


  it "accepts frames" do
    @msg.add "foo", "bar"
    expect(@msg.empty?).to be false
    expect(@msg.size).to eq 2
  end


  it "handles malformed requests" do
    @msg.add "foo", "bar"
    expect { @msg.send }.to raise_error Samwise::ResponseError
  end


  it "handles correct requests" do
    @msg.add "ping"
    expect(@msg.send).to eq true
  end

end
