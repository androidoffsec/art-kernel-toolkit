SECTIONS
{
    .art.plugins : {
        __art_plugins_start = . ;
        *(.art.plugins)
        __art_plugins_end = . ;
    }
}
