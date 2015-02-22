require 'samwise'

RSpec.describe Samwise::Connection do

  it "fails without explicit connect" do
    expect(Samwise::Connection.connected?)
      .to be false

    Samwise::Connection.close!
  end

  it "fails for unknown endpoints" do
    expect { Samwise::Connection.connect "endpoint" }
      .to raise_error (Samwise::ConnectionFailure)
  end

  it "connects to a known endpoint" do
    # TODO negotiate endpoint via config
    expect(Samwise::Connection.connect("ipc://../../sam_ipc"))
      .to be true

    expect(Samwise::Connection.connected?)
      .to be true

    Samwise::Connection.close!
    expect(Samwise::Connection.connected?)
      .to be false
  end


end
