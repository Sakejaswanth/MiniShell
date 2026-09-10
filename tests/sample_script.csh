# C-Shell Sample Script
# Tests batch execution, comments, built-ins, and file redirection

# Print greetings
echo "=== Welcome to C-Shell Script Execution ==="

# Display current working directory
pwd

# Set and check environment variables
setenv CSHELL_DEMO_KEY CSHELL_DEMO_VALUE
export CSHELL_EXPORTED=Active

# Test echo with escape sequences
echo "Testing escape characters: Line 1\nLine 2\tTabbed"

# Test built-in help summary
echo "Testing built-in commands..."
type cd
type echo
type pwd

# Test sequence of commands
echo "First in sequence" ; echo "Second in sequence"

echo "=== Script Execution Completed Successfully ==="
