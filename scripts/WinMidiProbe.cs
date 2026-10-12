// Read-only WinMM MIDI enumeration in the same Wine/Proton prefix as Rocksmith.
using System;
using System.Runtime.InteropServices;

class WinMidiProbe
{
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    struct MidiOutCaps
    {
        public ushort manufacturer, product;
        public uint version;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string name;
        public ushort technology, voices, notes, channels;
        public uint support;
    }

    [DllImport("winmm.dll")]
    static extern uint midiOutGetNumDevs();

    [DllImport("winmm.dll", CharSet = CharSet.Ansi)]
    static extern uint midiOutGetDevCapsA(UIntPtr device, out MidiOutCaps caps, uint size);

    static int Main()
    {
        uint count = midiOutGetNumDevs();
        Console.WriteLine("WinMM MIDI output devices: " + count);
        for (uint i = 0; i < count; i++)
        {
            MidiOutCaps caps;
            uint result = midiOutGetDevCapsA(new UIntPtr(i), out caps, (uint)Marshal.SizeOf(typeof(MidiOutCaps)));
            if (result != 0)
            {
                Console.Error.WriteLine("Enumeration failed for device " + i + ": " + result);
                return 1;
            }
            Console.WriteLine(i + ": " + caps.name);
        }
        return count > 0 ? 0 : 2;
    }
}
