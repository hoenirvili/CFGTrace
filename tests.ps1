ninja -C build -j2goliathtest
$process = Start-Process ctest -ArgumentList "-VV -j2 -C Debug" -NoNewWindow -PassThru -WorkingDirectory "build/tests"
$process.WaitForExit()
drmemory -quiet -no_follow_children -brief bin/goliathtest.exe