import os
import sys
import re

version = "1.0"

def main(args):
    try:
        if len(args) < 4 or len(args) % 2 == 1:
            print("Incorrect usage. Example usage:")
            print("Program <minify (true/false)> <output_file_path> <html_file_path_1> <macro_name_1> <html_file_path_2> <macro_name_2> ...")
            return

        # Check if HTML should be minified
        should_minify = args[0].lower() == 'true'

        # Output header file path
        output_file_path = args[1]

        if not os.path.exists(output_file_path):
            print(f"Error: The output file '{output_file_path}' does not exist.")
            return

        with open(output_file_path, 'r') as file:
            original_content = file.read()

        start_marker = "/* START HTML MACROS AREA */"
        end_marker = "/* END HTML MACROS AREA */"

        start_marker_index = original_content.find(start_marker)
        end_marker_index = original_content.find(end_marker)

        if start_marker_index == -1 or end_marker_index == -1 or start_marker_index >= end_marker_index:
            print(f"Error: Start and/or end marker not found in the output file, exptected markers {start_marker} and {end_marker}\n")
            return

        new_content = []
        new_content.append(original_content[:start_marker_index + len(start_marker)] + "\n")
        new_content.append("\n")
        new_content.append("/*\n")
        new_content.append(f" * Automatically generated code by 'HTML-to-Macro-String-Converter v{version}'\n")
        new_content.append(" * containing the HTML code converted to macros.\n")
        new_content.append(" * The parameters are:\n")
        new_content.append(f" *    Minify: {args[0]}\n")
        new_content.append(f" *    Output: {args[1]}\n")

        for i in range(2, len(args), 2):
            new_content.append(f" *    Input[{(i // 2)}]: {args[i]}, Macro: {args[i + 1]}\n")

        new_content.append(" *\n")
        new_content.append(" * Project Repository: https://github.com/moschiel/html-to-macro-string\n")
        new_content.append(" */\n\n")

        # Process each pair of HTML file path and macro name
        for i in range(2, len(args), 2):
            html_file_path = args[i]
            macro_name = args[i + 1]

            with open(html_file_path, 'r') as file:
                html_content = file.read()

            html_macro = convert_to_c_macro(html_content, macro_name, should_minify)
            new_content.append(html_macro + "\n\n")

        new_content.append(original_content[end_marker_index:])

        with open(output_file_path, 'w') as file:
            file.write(''.join(new_content))

        print("HTML macros successfully generated at:", output_file_path)
    except Exception as ex:
        print("Error processing HTML files:", str(ex))

def convert_to_c_macro(html_content, macro_name, should_minify):
    if should_minify:
        html_content = minify_html(html_content)
        html_content = html_content.replace('"', '\\"')
        html_content = f"\"{html_content}\""
        return f"#define {macro_name} {html_content}"
    else:
        macro_lines = [f"#define {macro_name} \\"]
        lines = html_content.splitlines()
        for line in lines:
            macro_lines.append("\"{}\\n\"\\".format(line.replace('"', '\\"')))
        macro_lines.append("\"\"")  # Add closing quote
        return '\n'.join(macro_lines)

def minify_html(html_content):
    # Preserve strings before processing to avoid misinterpreting '//' within strings as comments
    def preserve_strings(content):
        string_pattern = re.compile(r'(["\'])(?:\\.|[^\1])*?\1')
        string_matches = string_pattern.finditer(content)
        string_replacements = {}
        for i, match in enumerate(string_matches):
            original_string = match.group(0)
            placeholder = f"__STRING_PLACEHOLDER_{i}__"
            content = content.replace(original_string, placeholder, 1)
            string_replacements[placeholder] = original_string
        return content, string_replacements

    def restore_strings(content, replacements):
        for placeholder, original_string in replacements.items():
            content = content.replace(placeholder, original_string)
        return content

    # Minify contents within <script> sections
    script_pattern = re.compile(r'(<script\b[^>]*>)([\s\S]*?)(<\/script>)', re.IGNORECASE)
    matches = script_pattern.findall(html_content)

    for match in matches:
        script_tag, script_content, script_end_tag = match
        script_content, string_replacements = preserve_strings(script_content)
        minified_script_content = minify_script(script_content)
        minified_script_content = restore_strings(minified_script_content, string_replacements)
        minified_script_tag = script_tag + minified_script_content + script_end_tag
        html_content = html_content.replace(''.join(match), minified_script_tag)

    # Minify HTML outside <script> sections
    html_content, string_replacements = preserve_strings(html_content)
    html_content = re.sub(r'\s*(<[^>]+>)\s*', r'\1', html_content)  # Remove whitespace between tags
    html_content = re.sub(r'<!--.*?-->', '', html_content, flags=re.DOTALL)  # Remove comments
    html_content = re.sub(r'\s{2,}', ' ', html_content)  # Remove unnecessary whitespace
    html_content = re.sub(r'[\r\n]+', '', html_content)  # Remove line breaks
    html_content = restore_strings(html_content, string_replacements)

    return html_content.strip()

def minify_script(script_content):
    # Remove multi-line comments
    script_content = re.sub(r'/\*[\s\S]*?\*/', '', script_content)

    # Preserve strings to avoid interpreting them as comments
    string_pattern = re.compile(r'(["\'])(?:\\.|[^\1])*?\1')
    string_matches = string_pattern.finditer(script_content)
    string_replacements = {}

    for i, match in enumerate(string_matches):
        original_string = match.group(0)
        placeholder = f"__STRING_PLACEHOLDER_{i}__"
        script_content = script_content.replace(original_string, placeholder, 1)
        string_replacements[placeholder] = original_string

    # Remove single-line comments
    script_content = re.sub(r'//.*', '', script_content)

    # Remove unnecessary whitespace
    script_content = re.sub(r'\s+', ' ', script_content)

    # Remove whitespace around operators
    script_content = re.sub(r'\s*([{};:,])\s*', r'\1', script_content)

    # Restore strings
    for placeholder, original_string in string_replacements.items():
        script_content = script_content.replace(placeholder, original_string)

    return script_content.strip()

if __name__ == "__main__":
    main(sys.argv[1:])
