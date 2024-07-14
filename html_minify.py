import os
import sys
import re

version = "2.1"

def main(args):
    try:
        if len(args) < 2:
            print("Incorrect usage. Example usage:")
            print("Program <mode> [keep-line-break] <file_paths>")
            print("Modes: minify, minify-to-macro")
            return

        mode = args[0].lower()
        keep_line_break = 'keep-line-break' in args
        file_args_start_index = 2 if keep_line_break else 1

        if mode == "minify":
            if len(args) < file_args_start_index + 1:
                print("Incorrect usage for minify mode. Example usage:")
                print("Program minify [keep-line-break] <html_file_path_1> <html_file_path_2> ...")
                return
            minify_files(args[file_args_start_index:], keep_line_break)
        elif mode == "minify-to-macro":
            if len(args) < file_args_start_index + 2:
                print("Incorrect usage for minify-to-macro mode. Example usage:")
                print("Program minify-to-macro [keep-line-break] <macros_file_path> <html_file_path_1> <html_file_path_2> ...")
                return
            minify_to_macro(args[file_args_start_index], args[file_args_start_index + 1:], keep_line_break)
        else:
            print("Unknown mode. Please use 'minify' or 'minify-to-macro'.")
    except Exception as ex:
        print("Error processing HTML files:", str(ex))

def minify_files(file_paths, keep_line_break):
    for html_file_path in file_paths:
        try:
            with open(html_file_path, 'r') as file:
                html_content = file.read()
            minified_content = minify_html(html_content, keep_line_break)
            minified_file_path = os.path.join(os.path.dirname(html_file_path), f"min_{os.path.basename(html_file_path)}")
            with open(minified_file_path, 'w') as file:
                file.write(minified_content)
            print(f"Minified file generated at: {minified_file_path}")
        except Exception as ex:
            print(f"Error minifying file {html_file_path}: {str(ex)}")

def minify_to_macro(macros_file_path, html_file_paths, keep_line_break):
    try:
        if not os.path.exists(macros_file_path):
            print(f"Error: The macro file '{macros_file_path}' does not exist.")
            return

        with open(macros_file_path, 'r') as file:
            original_content = file.read()

        start_marker = "/* START HTML MACROS AREA */"
        end_marker = "/* END HTML MACROS AREA */"

        start_marker_index = original_content.find(start_marker)
        end_marker_index = original_content.find(end_marker)

        if start_marker_index == -1 or end_marker_index == -1 or start_marker_index >= end_marker_index:
            print(f"Error: Start and/or end marker not found in the macros file, expected markers {start_marker} and {end_marker}\n")
            return

        new_content = []
        new_content.append(original_content[:start_marker_index + len(start_marker)] + "\n")
        new_content.append("\n")
        new_content.append("/*\n")
        new_content.append(f" * Automatically generated macros by 'HTML-Minify-and-Macro-Converter v{version}'\n")
        new_content.append(" * containing the HTML code converted to macros.\n")
        new_content.append(" * The parameters are:\n")
        new_content.append(f" *    Output: {macros_file_path}\n")

        for i, html_file_path in enumerate(html_file_paths):
            macro_name = os.path.splitext(os.path.basename(html_file_path))[0].upper() + "_HTML"
            new_content.append(f" *    Input[{i + 1}]: {html_file_path}, Macro: {macro_name}\n")

        new_content.append(" *\n")
        new_content.append(" * Project Repository: https://github.com/moschiel/html-minify-and-macro-converter\n")
        new_content.append(" */\n\n")

        # Process each HTML file path
        for html_file_path in html_file_paths:
            macro_name = os.path.splitext(os.path.basename(html_file_path))[0].upper() + "_HTML"

            with open(html_file_path, 'r') as file:
                html_content = file.read()

            html_macro = convert_to_c_macro(html_content, macro_name, keep_line_break)
            new_content.append(html_macro + "\n\n")

        new_content.append(original_content[end_marker_index:])

        with open(macros_file_path, 'w') as file:
            file.write(''.join(new_content))

        print("HTML macros successfully generated at:", macros_file_path)
    except Exception as ex:
        print("Error processing HTML files:", str(ex))

def convert_to_c_macro(html_content, macro_name, keep_line_break):
    html_content = minify_html(html_content, keep_line_break)
    html_content = html_content.replace('"', '\\"')
    if keep_line_break:
        lines = html_content.split('\n')
        html_content = " \\\n".join([f"\"{line}\"" for line in lines])
    else:
        html_content = f"\"{html_content}\""
    return f"#define {macro_name} {html_content}"


def minify_html(html_content, keep_line_break):
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
        minified_script_content = minify_script(script_content, keep_line_break)
        minified_script_content = restore_strings(minified_script_content, string_replacements)
        minified_script_tag = script_tag + minified_script_content + script_end_tag
        html_content = html_content.replace(''.join(match), minified_script_tag)

    # Minify HTML outside <script> sections
    html_content, string_replacements = preserve_strings(html_content)
    html_content = re.sub(r'[ \t]*(<[^>]+>)[ \t]*', r'\1', html_content)  # Remove whitespace between tags, but keep line breaks
    html_content = re.sub(r'<!--.*?-->', '', html_content, flags=re.DOTALL)  # Remove comments
    if keep_line_break:
        html_content = re.sub(r'[ \t]{2,}', ' ', html_content)  # Remove unnecessary spaces but keep line breaks
        # Normalize line endings to \n
        html_content = re.sub(r'\r\n', '\n', html_content)  # Convert CRLF to LF
        html_content = re.sub(r'\r', '\n', html_content)    # Convert CR to LF
    else:
        html_content = re.sub(r'\s{2,}', ' ', html_content)  # Remove unnecessary whitespace
        html_content = re.sub(r'[\r\n]+', '', html_content)  # Remove line breaks
    
    # Remove all blank lines
    html_content = re.sub(r'^[ \t]*\n', '', html_content, flags=re.MULTILINE)  

    # Merge consecutive closing braces on separate lines into a single line
    html_content = re.sub(r'}\s*\n\s*}', r'}}', html_content)  
    
    html_content = restore_strings(html_content, string_replacements)

    return html_content.strip()

def minify_script(script_content, keep_line_break):
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

    if keep_line_break:
        script_content = re.sub(r'[ \t]+', ' ', script_content)  # Remove unnecessary spaces but keep line breaks
        script_content = re.sub(r'[ \t]*([{};:,])[ \t]*', r'\1', script_content)  # Remove spaces around operators but keep line breaks
    else:
        script_content = re.sub(r'\s+', ' ', script_content) # Remove unnecessary whitespace
        script_content = re.sub(r'\s*([{};:,])\s*', r'\1', script_content) # Remove whitespace around operators

    # Restore strings
    for placeholder, original_string in string_replacements.items():
        script_content = script_content.replace(placeholder, original_string)

    return script_content.strip()

if __name__ == "__main__":
    main(sys.argv[1:])
