echo "Emitting C++ Interface"
flatc -o ../cpp/shared --scoped-enums --reflect-names --gen-mutable -c noodles.fbs
flatc -o ../cpp/shared --scoped-enums --reflect-names --gen-mutable -c noodles_server.fbs
flatc -o ../cpp/shared --scoped-enums --reflect-names --gen-mutable -c noodles_client.fbs

echo "Emitting Python Interface"
flatc -o ../python/ --reflect-names --gen-mutable --no-includes -p noodles.fbs
flatc -o ../python/ --reflect-names --gen-mutable --no-includes -p noodles_server.fbs
flatc -o ../python/ --reflect-names --gen-mutable --no-includes -p noodles_client.fbs

echo "Emitting JS Interface"
flatc -o ../js/ --reflect-names --gen-mutable --js noodles.fbs
flatc -o ../js/ --reflect-names --gen-mutable --js noodles_server.fbs
flatc -o ../js/ --reflect-names --gen-mutable --js noodles_client.fbs
