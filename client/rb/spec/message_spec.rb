
RSpec.describe Samwise::Connection do

  before(:all) do
    Samwise::Connection.connect "ipc://../../sam_ipc"
  end

  after(:all) do
    Samwise::Connection.close!
  end

  it "creates a proper message" do
    msg = Samwise::Message.new
    expect(msg).to be_an_instance_of(Samwise::Message)

    expect(msg.empty?).to be true
    expect(msg.size).to eq 0

    expect { msg.send }.to raise_error Samwise::RequestMalformed
  end

  it "accepts frames" do
    msg = Samwise::Message.new
    msg.add "foo", "bar"

    expect(msg.empty?).to be false
    expect(msg.size).to eq 2
  end


  it "handles malformed requests" do
    msg = Samwise::Message.new
    msg.add "foo", "bar"
    expect { msg.send }.to raise_error Samwise::ResponseError
  end


  it "handles correct requests" do
    msg = Samwise::Message.new
    msg.add "ping"
    expect(msg.send).to eq true
  end

end
