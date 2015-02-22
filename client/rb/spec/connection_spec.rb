require 'samwise'

RSpec.describe Samwise::Connection do

  it "fails without explicit connect" do
    expect(Samwise::Connection.connected?)
      .to eq(false)

    Samwise::Connection.close!
  end

  it "fails for unknown endpoints" do
    expect { Samwise::Connection.connect "endpoint" }
      .to raise_error (Samwise::ConnectionFailure)
  end

  it "connects to a known endpoint" do
    # TODO negotiate endpoint via config
    expect(Samwise::Connection.connect("ipc://../../sam_ipc"))
      .to eq(true)

    expect(Samwise::Connection.connected?)
      .to eq(true)

    Samwise::Connection.close!
    expect(Samwise::Connection.connected?)
      .to eq(false)
  end


end
