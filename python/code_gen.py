def generate_header(args: list[str]) -> str:
    header_path = "/tmp/KernelArguments.hpp"
    member_names = [f"arg{i}" for i in range(len(args))]
    members = ";\n    ".join(f"{arg} {name}" for arg, name in zip(args, member_names)) + ";"
    as_tuple_members = ", ".join(member_names)

    header_content = f"""#pragma once
#include <tuple>

struct KernelArguments {{
    {members}

    auto as_tuple() const {{
        return std::tie({as_tuple_members});
    }}
}};
"""
    with open(header_path, "w") as header_file:
        header_file.write(header_content)
    
    return header_path
