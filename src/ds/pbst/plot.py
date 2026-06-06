import re

class TreeNode:
    def __init__(self, val, uid):
        self.val = val
        self.uid = uid # Unique ID for Graphviz
        self.left = None
        self.right = None

def parse_data(log):
    structs = {}
    current_addr = None
    root_addr = None

    # Parse raw text into structured dict
    for line in log.splitlines():
        line = line.strip()
        
        # Match struct header
        header_match = re.search(r'struct pbst @(0x[0-9a-f]+)', line)
        if header_match:
            current_addr = header_match.group(1)
            if root_addr is None: root_addr = current_addr
            structs[current_addr] = {'curr':{}, 'left':{}, 'right':{}}
            continue
            
        # Match fields: "type->field: value"
        if "->" in line:
            parts = line.split("->")
            node_type = parts[0].strip() # curr, left, right
            
            # Extract prop and value using split, then clean up
            rest = parts[1].split(":")
            prop = rest[0].strip()
            val = rest[1].strip()
            
            if current_addr:
                structs[current_addr][node_type][prop] = val

    return structs, root_addr

def build_tree(structs, addr, node_type):
    if addr not in structs: return None
    
    data = structs[addr][node_type]
    
    # Extract integer value safely
    raw_val = data.get('val', '-1')
    try:
        val = int(raw_val)
    except ValueError:
        val = -1
        
    if val == -1: return None

    # Create Node (Use address + type as Unique ID to avoid collisions)
    uid = f"node_{addr}_{node_type}"
    node = TreeNode(val, uid)
    
    # Logic for Packed Pointer Traversal
    # Left Child
    l_ptr = data.get('left')
    if l_ptr and l_ptr != '(nil)':
        if l_ptr == addr: # Points to self struct -> look in 'left' slot
            node.left = build_tree(structs, addr, 'left')
        else:             # Points to other struct -> look in 'curr' slot
            node.left = build_tree(structs, l_ptr, 'curr')

    # Right Child
    r_ptr = data.get('right')
    if r_ptr and r_ptr != '(nil)':
        if r_ptr == addr: # Points to self struct -> look in 'right' slot
            node.right = build_tree(structs, addr, 'right')
        else:             # Points to other struct -> look in 'curr' slot
            node.right = build_tree(structs, r_ptr, 'curr')
            
    return node

def write_dot_file(root):
    with open("tree.dot", "w", encoding="utf-8") as f:
        f.write("digraph BST {\n")
        f.write('    node [fontname="Arial", shape=circle, style=filled, fillcolor=white];\n')
        f.write('    edge [fontname="Arial"];\n')
        
        stack = [root]
        while stack:
            curr = stack.pop()
            if not curr: continue
            
            # Write Node Definition
            f.write(f'    {curr.uid} [label="{curr.val}"];\n')
            
            # Left Child
            if curr.left:
                f.write(f'    {curr.uid} -> {curr.left.uid} [label="L"];\n')
                stack.append(curr.left)
            else:
                # Invisible node for structure
                null_l = f"null_l_{curr.uid}"
                f.write(f'    {null_l} [shape=point, width=0.1, style=invis];\n')
                f.write(f'    {curr.uid} -> {null_l} [style=invis];\n')

            # Right Child
            if curr.right:
                f.write(f'    {curr.uid} -> {curr.right.uid} [label="R"];\n')
                stack.append(curr.right)
            else:
                # Invisible node for structure
                null_r = f"null_r_{curr.uid}"
                f.write(f'    {null_r} [shape=point, width=0.1, style=invis];\n')
                f.write(f'    {curr.uid} -> {null_r} [style=invis];\n')

        f.write("}\n")
    print("Successfully wrote 'tree.dot'")

log_data = ""
# The raw memory dump
with open("tree.raw", "r") as f:
    log_data = f.read()

# Execution
structs, root_addr = parse_data(log_data)
root_node = build_tree(structs, root_addr, 'curr')
write_dot_file(root_node)
