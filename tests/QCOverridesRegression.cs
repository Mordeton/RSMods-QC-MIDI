using System;
using System.IO;
using Newtonsoft.Json.Linq;
using RSMods;
class QCOverridesRegression {
    static void Check(bool condition, string message) { if (!condition) throw new Exception(message); }
    static int Main(string[] args) {
        try {
            JObject fixture = JObject.Parse(File.ReadAllText(args[0]));
            var store = new QCOverrides(); Check(store.Reload(fixture.ToString()), "Fixture load failed");
            foreach (JObject test in fixture["cases"]) {
                Check(store.Resolve((string)test["song"], (string)test["id"], "", (string)test["tone"]) == (string)test["expected"], "Shared fixture resolution mismatch");
                Check(store.Resolve((string)test["song"], "", (string)test["id"], (string)test["tone"]) == (string)test["expected"], "Arrangement fallback mismatch");
            }
            foreach (var invalid in fixture["invalid"]) {
                Check(!store.Reload(invalid.ToString()), "Invalid file accepted");
                Check(store.Resolve("IcedEarth-1776", "B655B6893F984ACFB054F35D73386A77", "", "Iced_Earth_Dist") == "Overdrive", "Last good mapping lost");
            }
            Console.WriteLine("C# shared QC override fixtures passed"); return 0;
        } catch (Exception ex) { Console.Error.WriteLine(ex); return 1; }
    }
}
