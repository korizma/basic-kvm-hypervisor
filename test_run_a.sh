set -e

make -C guest_a clean all
make -C host clean all

echo ""
echo "Test A:"
echo ""

./host/build/hypervisor --memory 4 --page 2 --guest guest_a/build/guest.img guest_a/build/guest.img guest_a/build/guest.img 