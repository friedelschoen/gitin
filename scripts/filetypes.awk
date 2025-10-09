#!/usr/bin/awk -f

BEGIN {
    print "package gitin"
    print "var filetypes = []FileType{"
}
/^#/ {
    next
}
$0 {
    printf("{ \"%s\", \"%s\", \"%s\" },\n", $1, $2, $3)
}
END { print "}" }
