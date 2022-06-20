const Path = require("path");
const FileSystem = require("fs");
const OS = require("os");
const { Command } = require("commander");
const SerialPort = require("serialport");

const program = new Command();
const package = JSON.parse(FileSystem.readFileSync(Path.join(__dirname, "../package.json")));

program.version(package.version, "-v, --version", "Print the current version");
program.helpOption('-h, --help', 'Display this help information');
program.showHelpAfterError();
program.showSuggestionAfterError();
program.configureOutput(
    {
        writeOut: (str) => process.stderr.write(`${str}`),
        writeErr: (str) => process.stderr.write(`${str}`)
    }
);

async function sleep(ms)
{
    return new Promise(resolve => setTimeout(resolve, ms));
}
////////////////////////////////////////////////
async function validate_serial_port(port)
{
    let ports = await SerialPort.list();

    for(let i = 0; i < ports.length; i++)
    {
        if(ports[i].path !== port)
            continue;

        if(typeof ports[i].vendorId != "string" || ports[i].vendorId.toUpperCase() !== "10C4")
            throw new Error("Invalid vendor ID");

        if(typeof ports[i].productId != "string" || ports[i].productId.toUpperCase() !== "EA60")
            throw new Error("Invalid product ID");

        return ports[i];
    }

    return false;
}
async function open_serial_port(port)
{
    if(!port)
        throw new Error("No port specified");

    return new Promise(
        function (resolve, reject)
        {
            port = new SerialPort(
                port,
                {
                    baudRate: 115200,
                    dataBits: 8,
                    parity: "none",
                    stopBits: 1,
                    autoOpen: false
                }
            );

            port.once(
                "open",
                function ()
                {
                    resolve(port);
                }
            );

            port.once(
                "error",
                function (err)
                {
                    reject(err);
                }
            );

            port.open();
        }
    );
}
async function serial_port_cmd(port, cmd)
{
    if(!port instanceof SerialPort || !port.isOpen)
        throw new Error("Invalid port");

    if(!cmd)
        throw new Error("No command specified");

    return new Promise(
        function (resolve, reject)
        {
            let resp = Buffer.alloc(0);
            let expectedLength = 4;

            port.flush();

            port.on(
                "data",
                function (buf)
                {
                    resp = Buffer.concat([resp, buf]);

                    if(resp.length < expectedLength)
                        return;

                    let magic = resp.readUInt16LE(0);
                    let cmd = resp.readUInt8(2);
                    let payloadLen = resp.readUInt8(3);

                    if(magic !== 0xFAC7)
                    {
                        port.removeAllListeners("data");

                        return reject(new Error("Invalid magic"));
                    }

                    if(payloadLen > 0)
                    {
                        expectedLength = payloadLen + 4;

                        if(resp.length < expectedLength)
                            return;
                    }

                    port.removeAllListeners("data");

                    return resolve(resp);
                }
            );

            port.write(
                cmd,
                function (err)
                {
                    if(err)
                    {
                        port.removeAllListeners("data");

                        return reject(err);
                    }
                }
            );

            port.drain();
        }
    );
}
async function cmd_set_dc(port, channel, dc)
{
    let cmd = Buffer.from([0xC7, 0xFA, 0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00]);

    cmd.writeUInt8(channel, 4);
    cmd.writeFloatLE(dc, 5);

    let resp = await serial_port_cmd(port, cmd);

    let magic = resp.readUInt16LE(0);
    let cmdID = resp.readUInt8(2);
    let payloadLen = resp.readUInt8(3);

    if(cmdID === 0xE0)
        throw new Error("Error setting DC");

    if(cmdID === 0x01)
        return true;
}
async function cmd_get_dc(port, channel)
{
    let cmd = Buffer.from([0xC7, 0xFA, 0x02, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00]);

    cmd.writeUInt8(channel, 4);

    let resp = await serial_port_cmd(port, cmd);

    let magic = resp.readUInt16LE(0);
    let cmdID = resp.readUInt8(2);
    let payloadLen = resp.readUInt8(3);

    if(cmdID === 0xE0)
        throw new Error("Error getting DC");

    if(cmdID === 0x02 && payloadLen === cmd.length - 4)
        return resp.readFloatLE(5);
}
async function cmd_get_voltage(port, channel)
{
    let cmd = Buffer.from([0xC7, 0xFA, 0x03, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00]);

    cmd.writeUInt8(channel, 4);

    let resp = await serial_port_cmd(port, cmd);

    let magic = resp.readUInt16LE(0);
    let cmdID = resp.readUInt8(2);
    let payloadLen = resp.readUInt8(3);

    if(cmdID === 0xE0)
        throw new Error("Error getting DC");

    if(cmdID === 0x03 && payloadLen === cmd.length - 4)
        return resp.readFloatLE(5);
}
async function cmd_get_temperature(port, channel)
{
    let cmd = Buffer.from([0xC7, 0xFA, 0x04, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00]);

    cmd.writeUInt8(channel, 4);

    let resp = await serial_port_cmd(port, cmd);

    let magic = resp.readUInt16LE(0);
    let cmdID = resp.readUInt8(2);
    let payloadLen = resp.readUInt8(3);

    if(cmdID === 0xE0)
        throw new Error("Error getting DC");

    if(cmdID === 0x04 && payloadLen === cmd.length - 4)
        return resp.readFloatLE(5);
}
async function cmd_set_freq(port, freq)
{
    let cmd = Buffer.from([0xC7, 0xFA, 0x05, 0x04, 0x00, 0x00, 0x00, 0x00]);

    cmd.writeFloatLE(freq, 4);

    let resp = await serial_port_cmd(port, cmd);

    let magic = resp.readUInt16LE(0);
    let cmdID = resp.readUInt8(2);
    let payloadLen = resp.readUInt8(3);

    if(cmdID === 0xE0)
        throw new Error("Error setting frequency");

    if(cmdID === 0x05)
        return true;
}
async function cmd_get_freq(port)
{
    let cmd = Buffer.from([0xC7, 0xFA, 0x06, 0x04, 0x00, 0x00, 0x00, 0x00]);

    let resp = await serial_port_cmd(port, cmd);

    let magic = resp.readUInt16LE(0);
    let cmdID = resp.readUInt8(2);
    let payloadLen = resp.readUInt8(3);

    if(cmdID === 0xE0)
        throw new Error("Error getting frequency");

    if(cmdID === 0x06 && payloadLen === cmd.length - 4)
        return resp.readFloatLE(4);
}
async function cmd_get_uid(port)
{
    let cmd = Buffer.from([0xC7, 0xFA, 0xF0, 0x00]);

    let resp = await serial_port_cmd(port, cmd);

    let magic = resp.readUInt16LE(0);
    let cmdID = resp.readUInt8(2);
    let payloadLen = resp.readUInt8(3);

    if(cmdID === 0xE0)
        throw new Error("Error getting UID");

    if(cmdID === 0xF0 && payloadLen === 8)
    {
        let str = "";

        for(let i = 7; i >= 0; i--)
            str += resp.readUInt8(i + 4).toString(16).padStart(2, "0").toUpperCase() + (i === 4 ? "-" : "");

        return str;
    }
}

async function run()
{
    let opts = program.opts();

    if(!opts.port)
    {
        console.log("Invalid options provided");
        console.log("Invalid serial port");

        return process.exit(1);
    }

    let port_details

    try
    {
        port_details = await validate_serial_port(opts.port);
    }
    catch(e)
    {
        console.log("Invalid options provided");
        console.log("Serial port does not match criteria");
        console.log(e);

        return process.exit(1);
    }

    if(!port_details)
    {
        console.log("Invalid options provided");
        console.log("Serial port does not exist");

        return process.exit(1);
    }

    let port;

    try
    {
        port = await open_serial_port(opts.port);
    }
    catch(e)
    {
        console.log("Error opening serial port");
        console.log(e);

        return process.exit(1);
    }

    if(typeof opts.voltage === "number")
    {
        if(opts.voltage < 0 || opts.voltage > 5)
        {
            console.log("Invalid options provided");
            console.log("Invalid voltage channel (0 < chan < 6)");

            return process.exit(1);
        }

        console.log((await cmd_get_voltage(port, opts.voltage)) + " mV");

        port.close();
        return process.exit(0);
    }

    if(typeof opts.temp === "number")
    {
        if(opts.temp < 0 || opts.temp > 1)
        {
            console.log("Invalid options provided");
            console.log("Invalid temperature channel (0 < chan < 2)");

            return process.exit(1);
        }

        console.log((await cmd_get_temperature(port, opts.temp)) + " C");

        port.close();
        return process.exit(0);
    }

    if(typeof opts.freq === "number")
    {
        if(opts.freq < 0)
        {
            console.log("Invalid options provided");
            console.log("Invalid frequency");

            return process.exit(1);
        }

        await cmd_set_freq(port, opts.freq);

        port.close();
        return process.exit(0);
    }

    if(typeof opts.channel === "number")
    {
        if(opts.channel < 0 || opts.channel > 6)
        {
            console.log("Invalid options provided");
            console.log("Invalid channel (0 < chan < 7)");

            return process.exit(1);
        }

        if(typeof opts.dutyCycle === "number")
        {
            if(opts.dutyCycle < 0 || opts.dutyCycle > 100)
            {
                console.log("Invalid options provided");
                console.log("Invalid duty cycle (0 < dc < 100)");

                return process.exit(1);
            }

            await cmd_set_dc(port, opts.channel, opts.dutyCycle / 100);

            port.close();
            return process.exit(0);
        }

        console.log((await cmd_get_dc(port, opts.channel)) * 100 + " %");

        port.close();
        return process.exit(0);
    }

    console.log("USB Serial number: " + port_details.serialNumber.toUpperCase());
    console.log("Unique ID: " + await cmd_get_uid(port));

    console.log("Frequency: " + (await cmd_get_freq(port)) + " Hz");

    let str;

    str = "Duty cycle: ";

    for(let i = 0; i < 7; i++)
        str += ((await cmd_get_dc(port, i)) * 100).toFixed(2) + "%" + (i === 6 ? "" : ", ");

    console.log(str);

    str = "Voltage: ";

    for(let i = 0; i < 6; i++)
        str += (await cmd_get_voltage(port, i)).toFixed(2) + " mV" + (i === 5 ? "" : ", ");

    console.log(str);

    str = "Temperature: ";

    for(let i = 0; i < 2; i++)
        str += (await cmd_get_temperature(port, i)).toFixed(2) + " C" + (i === 1 ? "" : ", ");

    console.log(str);

    port.close();
    process.exit(0);
}

async function main()
{
    let defaultPort;

    if(OS.platform() === "win32")
        defaultPort = "COM1";
    else if(OS.platform() === "linux")
        defaultPort = "/dev/ttyUSB0";

    program
        .option("-p, --port <port>", "Serial port to use", defaultPort)
        .option("-d, --duty-cycle <dc>", "Set the duty cycle, requires -c", parseFloat)
        .option("-c, --channel <chan>", "Set the channel, if -d is not set, reads back the current value", parseInt)
        .option("-m, --voltage <chan>", "Read this voltage channel", parseInt)
        .option("-t, --temp <chan>", "Read this temperature channel", parseInt)
        .option("-f, --freq <freq>", "Set the PWM frequency", parseFloat)
        .option("-V, --verbose", "Print debugging information")
        .action(run);

    await program.parseAsync();
}

main();