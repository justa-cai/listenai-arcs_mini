    .shell : ALIGN(4) {
        . = ALIGN(4);
        _shell_items_start = .;
        KEEP(* (.shell.user))
        KEEP(* (.shell.priv))
        . = ALIGN(4);
    } >ROM AT>ROM

    .shell_command  :
    {
        _shell_command_start = .;
        KEEP (*(shellCommand))
        _shell_command_end = .;
    } >ROM AT>ROM
