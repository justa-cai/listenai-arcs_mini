/* MCP工具静态注册段 */
.mcp_tool : {
    . = ALIGN(4);
    __mcp_tool_start = .;
    KEEP (*(.mcp_tool))
    . = ALIGN(4);
    __mcp_tool_end = .;
} >ROM AT>ROM
