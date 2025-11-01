#!/bin/bash
# Script to test HDEPanel with different languages

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "HDEPanel Language Tester"
echo "========================"
echo ""
echo "Select a language to test:"
echo "  1) English (en)"
echo "  2) Dutch / Nederlands (nl)"
echo "  3) Arabic / العربية (ar)"
echo "  4) System default"
echo ""
read -p "Enter choice (1-4): " choice

case $choice in
    1)
        echo "Starting HDEPanel in English..."
        LANGUAGE=en "$SCRIPT_DIR/hdepanel-launch.sh"
        ;;
    2)
        echo "Starting HDEPanel in Dutch..."
        LANGUAGE=nl "$SCRIPT_DIR/hdepanel-launch.sh"
        ;;
    3)
        echo "Starting HDEPanel in Arabic..."
        LANGUAGE=ar "$SCRIPT_DIR/hdepanel-launch.sh"
        ;;
    4)
        echo "Starting HDEPanel with system default language..."
        "$SCRIPT_DIR/hdepanel-launch.sh"
        ;;
    *)
        echo "Invalid choice. Using system default."
        "$SCRIPT_DIR/hdepanel-launch.sh"
        ;;
esac

