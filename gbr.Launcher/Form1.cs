using System;
using System.Diagnostics;
using System.IO;
using System.IO.Pipes;
using System.Linq;
using System.Windows.Forms;
using GWCA.Memory;

namespace gbr.Launcher
{
    public partial class Form1 : Form
    {
        private Process[] procs;

        public Form1()
        {
            InitializeComponent();
        }

        private void label1_Click(object sender, EventArgs e)
        {

        }

        private void button1_Click(object sender, EventArgs e)
        {
            string dllFile = "gbr.InProcess.dll";

            if (!File.Exists(dllFile))
            {
                MessageBox.Show($"Cannot find {dllFile}", "gbr.Launcher Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            var selected = listBox1.SelectedItems.Cast<GwProcess>();
            var mems = selected.Select(item => new GWCAMemory(item.Process));

            foreach (var m in mems)
            {
                IntPtr dllReturn;
                var result = m.LoadModule(dllFile, out dllReturn);
                if (result != GWCAMemory.LOADMODULERESULT.SUCCESSFUL
                    || dllReturn == IntPtr.Zero)
                {
                    MessageBox.Show("Error loading DLL", "gbr.Launcher Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private void button2_Click(object sender, EventArgs e)
        {
            var selected = listBox1.SelectedItems.Cast<GwProcess>();

            foreach (var item in selected)
            {
                using (var client = new NamedPipeClientStream("gbr_" + item.Name))
                {
                    client.Connect(3000);

                    if (client.IsConnected)
                    {
                        client.WriteByte(5); // Terminate in the CommandType enum
                        client.Flush();
                        client.Close();
                    }
                }
            }
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            procs = Process.GetProcessesByName("Gw");
            if (procs.Length > 0)
            {
                var charnameAddr = GetCharnameAddress(procs[0]);

                var list = procs
                    .Select(proc =>
                    {
                        var mem = new GWCAMemory(proc);
                        return new GwProcess { Process = proc, Name = mem.ReadWString(charnameAddr, 30) };
                    })
                    .ToList();

                listBox1.DataSource = list;
                listBox1.DisplayMember = "Name";
                listBox1.ValueMember = "Process";
            }
        }

        private IntPtr GetCharnameAddress(Process process)
        {
            var firstprocmems = new GWCAMemory(process);
            firstprocmems.InitScanner(new IntPtr(0x401000), 0x49A000);
            var charnameAddr = firstprocmems.ScanForPtr(new byte[] { 0x6A, 0x14, 0x8D, 0x96, 0xBC }, 0x9, true);
            firstprocmems.TerminateScanner();

            return charnameAddr;
        }
    }

    public class GwProcess
    {
        public string Name { get; set; }
        public Process Process { get; set; }
    }
}
