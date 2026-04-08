#!/usr/bin/env python3
# builder.py

import yaml
import re
import argparse
import os
import sys
from typing import List
from dataclasses import dataclass

# Serialize/Deserialize templates.
cpp_serialize_deserialize_templates : str = """
// Serialize/Deserialize templates.
template<typename T>
static void serialize(uint8_t* buffer, T value, int& offset) {
    static_assert(sizeof(T) <= 8, "Type size too large");
    for(size_t i = 0; i < sizeof(T); ++i) {
        buffer[offset++] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
}

template<typename T>
static void deserialize(const uint8_t* buffer, T& value, int& offset) {
    static_assert(sizeof(T) <= 8, "Type size too large");
    value = 0;
    for(size_t i = 0; i < sizeof(T); ++i) {
        value |= static_cast<T>(static_cast<uint64_t>(buffer[offset + i]) << (i * 8));
    }
    offset += sizeof(T);
}

// For bool values
template<>
void serialize<bool>(uint8_t* buffer, bool value, int& offset) {
    buffer[offset++] = value ? 1 : 0;
}

template<>
void deserialize<bool>(const uint8_t* buffer, bool& value, int& offset) {
    value = buffer[offset++] != 0;
}

// For arrays
template<typename T, size_t N>
void serialize_array(uint8_t* buffer, T (arr)[N], int& offset) {
    for(size_t i = 0; i < N; ++i) {
        serialize<T>(buffer, arr[i], offset);
    }
}

template<typename T, size_t N>
void deserialize_array(const uint8_t* buffer, T (&arr)[N], int& offset) {
    for(size_t i = 0; i < N; ++i) {
        deserialize<T>(buffer, arr[i], offset);
    }
}
"""


@dataclass
class Parameter:
    name: str
    type: str
    is_array: bool = False
    array_size: int = 0

@dataclass
class Function:
    name: str
    return_type: str
    parameters: List[Parameter]

class RPCGenerator:
    def __init__(self, yaml_file: str, output_dir: str = "."):
        """Initialize RPC generator

        Args:
            yaml_file: Path to YAML configuration file
            output_dir: Output directory, defaults to current directory
        """
        self.yaml_file = yaml_file
        self.output_dir = output_dir

        # Ensure output directory exists
        os.makedirs(output_dir, exist_ok=True)

        # Load configuration file
        with open(yaml_file, 'r') as f:
            self.config = yaml.safe_load(f)

        self.service_name = self.config['service']
        self.functions = []

        # Parse function definitions
        for func in self.config['functions']:
            params = []
            for param in func.get('parameters', []):
                match = re.match(r'(\w+)\[(\d+)\]', param['type'])
                if match:
                    base_type, size = match.groups()
                    params.append(Parameter(
                        name=param['name'],
                        type=base_type,
                        is_array=True,
                        array_size=int(size)
                    ))
                else:
                    params.append(Parameter(
                        name=param['name'],
                        type=param['type'],
                        is_array=False
                    ))

            self.functions.append(Function(
                name=func['name'],
                return_type=func['return_type'],
                parameters=params
            ))

    def generate_all_code(self) -> None:
        """Generate all code files"""
        print(f"Generating RPC code from {self.yaml_file}...")
        print(f"Service: {self.service_name}")
        print(f"Functions: {[f.name for f in self.functions]}")

        self.generate_client_code()
        self.generate_server_code()
        print(f"Code generated successfully in directory: {self.output_dir}")

    def generate_client_code(self) -> None:
        """Generate client code"""
        header_content = self._generate_client_header()
        header_path = os.path.join(self.output_dir, f"{self.service_name}Client.h")
        with open(header_path, 'w') as f:
            f.write(header_content)

        impl_content = self._generate_client_impl()
        impl_path = os.path.join(self.output_dir, f"{self.service_name}Client.cpp")
        with open(impl_path, 'w') as f:
            f.write(impl_content)

        print(f"  Generated client code: {header_path}, {impl_path}")

    def generate_server_code(self) -> None:
        """Generate server code"""
        header_content = self._generate_server_header()
        header_path = os.path.join(self.output_dir, f"{self.service_name}Server.h")
        with open(header_path, 'w') as f:
            f.write(header_content)

        impl_content = self._generate_server_impl()
        impl_path = os.path.join(self.output_dir, f"{self.service_name}Server.cpp")
        with open(impl_path, 'w') as f:
            f.write(impl_content)

        print(f"  Generated server code: {header_path}, {impl_path}")

    def _generate_client_header(self) -> str:
        """generate client header"""
        functions_decl = []
        for func in self.functions:
            param_list = []
            for param in func.parameters:
                if param.is_array:
                    param_list.append(f"{param.type} {param.name}[{param.array_size}]")
                else:
                    param_list.append(f"{param.type} {param.name}")

            param_str = ", ".join(param_list)
            functions_decl.append(f"    {func.return_type} {func.name}({param_str});")

        functions_str = "\n".join(functions_decl)

        return f"""// Auto-generated RPC Client for {self.service_name}
#ifndef {self.service_name.upper()}_CLIENT_H
#define {self.service_name.upper()}_CLIENT_H

#include <cstdint>
#include <cstring>

class {self.service_name}Client {{
public:
    // I/O operations are passed by the user.
    using SendFunc = bool(*)(const uint8_t*, int);
    using RecvFunc = bool(*)(uint8_t*, int);

    {self.service_name}Client(SendFunc send_func, RecvFunc recv_func);

    // RPC function declaration
{functions_str}

private:
    SendFunc send_func_;
    RecvFunc recv_func_;

}};

#endif // {self.service_name.upper()}_CLIENT_H
"""

    def _generate_client_impl(self) -> str:
        """generate client implementation"""
        function_impls = []
        for i, func in enumerate(self.functions):
            func_impl = self._generate_client_function_impl(func, i)
            function_impls.append(func_impl)

        functions_str = "\n".join(function_impls)

        return f"""// Auto-generated RPC Client Implementation for {self.service_name}
#include "{self.service_name}Client.h"

{cpp_serialize_deserialize_templates}

{self.service_name}Client::{self.service_name}Client(SendFunc send_func, RecvFunc recv_func)
    : send_func_(send_func), recv_func_(recv_func) {{
}}

{functions_str}
"""

    def _generate_client_function_impl(self, func: Function, func_id: int) -> str:
        """generate client implementation for single function"""
        # Calculate total buffer size expression
        total_size_expr = "1"  # function id
        for param in func.parameters:
            if param.is_array:
                total_size_expr += f" + sizeof({param.type}) * {param.array_size}"
            else:
                total_size_expr += f" + sizeof({param.type})"

        # Return value size expression
        return_size_expr = f"sizeof({func.return_type})" if func.return_type != "void" else "0"

        # Generate parameter serialization code
        pack_code = []
        pack_code.append("    offset = 0;")
        pack_code.append("    // first byte is function id")
        pack_code.append(f"    buffer[0] = 0x{func_id+1:02X}; ")
        pack_code.append("    offset = 1;")
        pack_code.append("")

        for param in func.parameters:
            if param.is_array:
                pack_code.append(f"    // Serialize array {param.name}")
                pack_code.append(f"    serialize_array<{param.type}, {param.array_size}>(buffer, {param.name}, offset);")
            else:
                pack_code.append(f"    serialize<{param.type}>(buffer, {param.name}, offset);")

        pack_str = "\n".join(pack_code)

        # Generate return value deserialization code
        unpack_code = ""
        if func.return_type != "void":
            unpack_code = f"""
    // Deserialize return value
    {func.return_type} result;
    offset = 0;
    deserialize<{func.return_type}>(recv_buffer, result, offset);
    return result;"""
        else:
            unpack_code = "    return;"

        return f"""// {func.name} function implementation
{func.return_type} {self.service_name}Client::{func.name}({self._generate_parameter_list(func.parameters)}) {{
    // Calculate buffer size
    const int total_size = {total_size_expr};
    const int return_size = {return_size_expr};

    uint8_t buffer[total_size];
    int offset = 0;

{pack_str}

    // Send request
    if (!send_func_(buffer, total_size)) {{
        // Handle send failure
        return {self._get_default_value(func.return_type)};
    }}

    // Receive response
    uint8_t recv_buffer[return_size];
    if (!recv_func_(recv_buffer, return_size)) {{
        // Handle receive failure
        return {self._get_default_value(func.return_type)};
    }}
{unpack_code}
}}
"""

    def _generate_server_header(self) -> str:
        """Generate server header"""
        functions_decl = []
        for func in self.functions:
            param_list = []
            for param in func.parameters:
                if param.is_array:
                    param_list.append(f"{param.type} {param.name}[{param.array_size}]")
                else:
                    param_list.append(f"{param.type} {param.name}")

            param_str = ", ".join(param_list)
            functions_decl.append(f"    virtual {func.return_type} {func.name}({param_str}) = 0;")

        functions_str = "\n".join(functions_decl)

        return f"""// Auto-generated RPC Server for {self.service_name}
#ifndef {self.service_name.upper()}_SERVER_H
#define {self.service_name.upper()}_SERVER_H

#include <cstdint>
#include <cstring>

class {self.service_name}Server {{
public:
    virtual ~{self.service_name}Server() = default;

    // Pure virtual functions to be implemented by user
{functions_str}

    // Entry point to handle a request
    bool handle_request(const uint8_t* request, int request_size, uint8_t* response, int& response_size);

}};

#endif // {self.service_name.upper()}_SERVER_H
"""

    def _generate_server_impl(self) -> str:
        """Generate server implementation"""
        case_handlers = []
        for i, func in enumerate(self.functions):
            handler = self._generate_server_case_handler(func, i)
            case_handlers.append(handler)

        handlers_str = "\n".join(case_handlers)

        return f"""// Auto-generated RPC Server Implementation for {self.service_name}
#include "{self.service_name}Server.h"

{cpp_serialize_deserialize_templates}

bool {self.service_name}Server::handle_request(const uint8_t* request, int request_size, uint8_t* response, int& response_size) {{
    if (request_size < 1) {{
        return false;
    }}

    uint8_t func_id = request[0];
    int offset = 1;

    switch (func_id) {{
{handlers_str}
        default:
            return false;
    }}

    return true;
}}
"""

    def _generate_server_case_handler(self, func: Function, func_id: int) -> str:
        """Generate server case handler"""
        # Generate parameter deserialization code
        unpack_code = []
        param_names = []

        for param in func.parameters:
            if param.is_array:
                unpack_code.append(f"            {param.type} {param.name}[{param.array_size}];")
                unpack_code.append(f"            deserialize_array<{param.type}, {param.array_size}>(request, {param.name}, offset);")
            else:
                unpack_code.append(f"            {param.type} {param.name};")
                unpack_code.append(f"            deserialize<{param.type}>(request, {param.name}, offset);")

            param_names.append(param.name)

        unpack_str = "\n".join(unpack_code)

        # Generate function call and return value serialization code
        if func.return_type != "void":
            param_str = ", ".join(param_names)
            return_code = f"""            // Call actual function
            {func.return_type} result = {func.name}({param_str});

            // Serialize return value
            offset = 0;
            serialize<{func.return_type}>(response, result, offset);
            response_size = offset;"""
        else:
            param_str = ", ".join(param_names)
            return_code = f"""            // Call actual function
            {func.name}({param_str});
            response_size = 0;"""

        return f"""        case 0x{func_id+1:02X}:  // {func.name}
        {{
{unpack_str}
{return_code}
        }}
        break;"""

    def _generate_parameter_list(self, parameters: List[Parameter]) -> str:
        """Generate parameter list string"""
        param_strs = []
        for param in parameters:
            if param.is_array:
                param_strs.append(f"{param.type} {param.name}[{param.array_size}]")
            else:
                param_strs.append(f"{param.type} {param.name}")

        return ", ".join(param_strs)

    def _get_default_value(self, type_str: str) -> str:
        """Get default value"""
        default_map = {
            'int8_t': '0', 'uint8_t': '0',
            'int16_t': '0', 'uint16_t': '0',
            'int32_t': '0', 'uint32_t': '0',
            'int64_t': '0', 'uint64_t': '0',
            'bool': 'false',
            'void': ''
        }
        return default_map.get(type_str, '0')

def parse_arguments():
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(
        description="Generate RPC client and server code from YAML configuration.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s -i rpc_config.yaml
  %(prog)s -i config/my_rpc.yaml -o generated/
  %(prog)s --input rpc.yaml --output src/generated --verbose
        """
    )

    # Required argument: input YAML file
    parser.add_argument(
        "-i", "--input",
        required=True,
        help="Input YAML configuration file (required)"
    )

    # Optional argument: output directory
    parser.add_argument(
        "-o", "--output",
        default=".",
        help="Output directory for generated files (default: current directory)"
    )

    # Optional argument: verbose mode
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose output"
    )

    # Optional argument: generate only client or server
    parser.add_argument(
        "--client-only",
        action="store_true",
        help="Generate only client code"
    )

    parser.add_argument(
        "--server-only",
        action="store_true",
        help="Generate only server code"
    )

    # Optional argument: overwrite existing files
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite existing files without confirmation"
    )

    return parser.parse_args()

def validate_yaml_file(yaml_file: str) -> bool:
    """Validate that YAML file exists and has correct format"""
    if not os.path.exists(yaml_file):
        print(f"Error: YAML file '{yaml_file}' does not exist.")
        return False

    if not yaml_file.endswith(('.yaml', '.yml')):
        print(f"Warning: File '{yaml_file}' does not have .yaml or .yml extension.")

    try:
        with open(yaml_file, 'r') as f:
            yaml.safe_load(f)
        return True
    except yaml.YAMLError as e:
        print(f"Error: Invalid YAML format in '{yaml_file}': {e}")
        return False
    except Exception as e:
        print(f"Error reading '{yaml_file}': {e}")
        return False

def main():
    """Main function"""
    # Parse command line arguments
    args = parse_arguments()

    # Validate YAML file
    if not validate_yaml_file(args.input):
        sys.exit(1)

    # Create generator instance
    generator = RPCGenerator(args.input, args.output)

    # Generate code according to options
    if args.client_only:
        if args.verbose:
            print(f"Generating only client code for service: {generator.service_name}")
        generator.generate_client_code()
    elif args.server_only:
        if args.verbose:
            print(f"Generating only server code for service: {generator.service_name}")
        generator.generate_server_code()
    else:
        # Generate all code
        if args.verbose:
            print(f"Generating both client and server code for service: {generator.service_name}")
        generator.generate_all_code()

    if args.verbose:
        print("Code generation completed successfully!")

if __name__ == "__main__":
    main()