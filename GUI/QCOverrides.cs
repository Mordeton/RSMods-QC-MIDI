using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Newtonsoft.Json.Linq;

namespace RSMods {
    // In-game editor is the only writer. The GUI keeps its last valid snapshot on read errors.
    internal sealed class QCOverrides {
        internal static readonly string[] Roles = { "Clean", "Overdrive", "Distortion", "Mod", "Solo" };
        private Dictionary<string, Dictionary<string, string>> entries = new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
        private string contents;
        private string loadedPath;
        public string Error { get; private set; } = "";
        internal static string NormalizeId(string value) {
            string id = new string((value ?? "").Where(c => c != '-' && c != '{' && c != '}' && !char.IsWhiteSpace(c)).ToArray()).ToUpperInvariant();
            return id.Length == 32 && id.All(c => (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) ? id : "";
        }
        private static string Key(string song, string id) { return song + "\n" + NormalizeId(id); }
        public bool Reload(string text) {
            try {
                JObject root = JObject.Parse(text);
                if (root["version"]?.Type != JTokenType.Integer || (int)root["version"] != 1 || !(root["entries"] is JArray)) throw new Exception("Unsupported QC override file");
                var next = new Dictionary<string, Dictionary<string, string>>(StringComparer.Ordinal);
                foreach (JObject entry in (JArray)root["entries"]) {
                    if (entry["songKey"]?.Type != JTokenType.String || entry["arrangementId"]?.Type != JTokenType.String) throw new Exception("Invalid override identity");
                    string song = (string)entry["songKey"], id = NormalizeId((string)entry["arrangementId"]);
                    if (string.IsNullOrEmpty(song) || id.Length == 0 || !(entry["tones"] is JObject)) throw new Exception("Invalid override identity");
                    var tones = new Dictionary<string, string>(StringComparer.Ordinal);
                    foreach (var tone in ((JObject)entry["tones"]).Properties()) {
                        if (tone.Name.Length == 0 || tone.Value.Type != JTokenType.String || !Roles.Contains((string)tone.Value)) throw new Exception("Invalid tone role");
                        tones.Add(tone.Name, (string)tone.Value);
                    }
                    next.Add(Key(song, id), tones);
                }
                entries = next; Error = ""; return true;
            } catch (Exception ex) { Error = ex.Message; return false; }
        }
        public bool Refresh(string path) {
            try {
                if (loadedPath != path) { entries.Clear(); contents = null; loadedPath = path; }
                string text = File.Exists(path) ? File.ReadAllText(path) : "{\"version\":1,\"entries\":[]}";
                if (text == contents) return false;
                contents = text; Reload(text); return true;
            } catch (Exception ex) { Error = ex.Message; return true; }
        }
        public string Resolve(string song, string persistent, string arrangement, string tone) {
            string id = NormalizeId(persistent);
            if (id.Length == 0) id = NormalizeId(arrangement);
            Dictionary<string, string> tones; string role;
            return id.Length != 0 && entries.TryGetValue(Key(song, id), out tones) && tones.TryGetValue(tone, out role) ? role : "";
        }
    }
}
