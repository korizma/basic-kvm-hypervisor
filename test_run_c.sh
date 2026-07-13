set -e

make -C guest clean all
make -C host clean all

echo ""
echo "Test C:"
echo ""

./host/build/hypervisor --memory 4 --page 2 --guest guest/build/guest.img guest/build/guest.img guest/build/guest.img --file testfiles/testc.txt