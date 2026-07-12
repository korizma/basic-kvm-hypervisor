set -e

make -C guest_b clean all
make -C host clean all

echo ""
echo "Running hypervisor:"
echo ""

./host/build/hypervisor --memory 4 --page 2 --guest guest_b/build/guest.img --file testfiles/testb.txt