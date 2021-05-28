echo "Emitting C++ Interface"
flatc -I ../message_spec/interface -o . --scoped-enums --reflect-names --gen-mutable -c ../message_spec/interface/noodles.fbs
flatc -I ../message_spec/interface -o . --scoped-enums --reflect-names --gen-mutable -c ../message_spec/interface/noodles_server.fbs
flatc -I ../message_spec/interface -o . --scoped-enums --reflect-names --gen-mutable -c ../message_spec/interface/noodles_client.fbs
