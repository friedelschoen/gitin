#!/usr/bin/awk -f

BEGIN {
    print "package preview"
    print "var Filetypes = []FileType{"
}
/^#/ {
    next
}
$0 {
    prev = $3
    if (!prev) prev = "nil"
    printf("{ \"%s\", \"%s\", %s, \"%s\" },\n", $1, $2, prev, $4)
}
END { print "}" }
