echo "Emitting C++ Interface"
flatc -o . --scoped-enums --reflect-names --gen-mutable -c ../message_spec/interface/noodles.fbs

echo "Creating Binary Schema"
flatc -o . -b --schema --bfbs-builtins  ../message_spec/interface/noodles.fbs

cat << EOF > noodles_bfbs.h
#pragma once
namespace noodles {
EOF

xxd -i noodles.bfbs >> noodles_bfbs.h

cat << EOF >> noodles_bfbs.h
}
EOF
