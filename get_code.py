import json

log_path = "/home/moon/.gemini/antigravity/brain/5a98b736-215e-4665-ae0f-97ffef7ce518/.system_generated/logs/overview.txt"
out_path = "/home/moon/Documents/Nyx AntiGravity/natural_compare_extracted.txt"

with open(log_path, "r") as f, open(out_path, "w") as out:
    for i, line in enumerate(f):
        if "replace_file_content" in line and "_natural_compare" in line:
            try:
                data = json.loads(line)
                for tool in data.get("tool_calls", []):
                    if tool.get("name") == "replace_file_content":
                        args = tool.get("args", {})
                        content = args.get("ReplacementContent", "")
                        if "_natural_compare" in content:
                            out.write(f"--- MATCH {i} ---\n")
                            out.write(content)
                            out.write("\n\n")
            except Exception as e:
                out.write(f"Error parsing line {i}: {str(e)}\n")
