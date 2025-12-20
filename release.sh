#!/bin/bash
# OTS Release Tagging Script
# Creates date-based version tags: YYYY-MM-DD.revision

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get current date
DATE=$(date +%Y-%m-%d)

# Function to get next revision number for today
get_next_revision() {
    local prefix=$1
    local pattern="${prefix}${DATE}."
    
    # Get all tags matching today's date for this project
    local tags=$(git tag -l "${pattern}*" | sort -V)
    
    if [ -z "$tags" ]; then
        echo "1"
        return
    fi
    
    # Get the highest revision number
    local last_tag=$(echo "$tags" | tail -n 1)
    local last_revision=$(echo "$last_tag" | sed "s/${pattern}//")
    local next_revision=$((last_revision + 1))
    
    echo "$next_revision"
}

# Function to create tag
create_tag() {
    local project=$1
    local prefix=$2
    local description=$3
    
    local revision=$(get_next_revision "$prefix")
    local tag="${prefix}${DATE}.${revision}"
    
    echo -e "${BLUE}Creating tag for ${project}:${NC} ${GREEN}${tag}${NC}"
    
    # Create annotated tag
    git tag -a "$tag" -m "${project} release ${DATE}.${revision}

${description}

Tagged projects:
$(git status --short)"
    
    echo -e "${GREEN}✓${NC} Tag created: ${tag}"
    echo -e "  To push: ${YELLOW}git push origin ${tag}${NC}"
    
    return 0
}

# Function to update version in files
update_version() {
    local project=$1
    local version=$2
    
    case $project in
        userscript)
            echo -e "${BLUE}Updating userscript version to ${version}${NC}"
            
            # Update package.json
            if [ -f "ots-userscript/package.json" ]; then
                sed -i "s/\"version\": \".*\"/\"version\": \"${version}\"/" ots-userscript/package.json
                echo -e "  ${GREEN}✓${NC} ots-userscript/package.json"
            fi
            
            # Update main.user.ts VERSION constant
            if [ -f "ots-userscript/src/main.user.ts" ]; then
                sed -i "s/const VERSION = '.*'/const VERSION = '${version}'/" ots-userscript/src/main.user.ts
                echo -e "  ${GREEN}✓${NC} ots-userscript/src/main.user.ts"
            fi
            
            # Rebuild userscript to update header
            echo -e "${BLUE}Rebuilding userscript...${NC}"
            (cd ots-userscript && npm run build > /dev/null 2>&1)
            echo -e "  ${GREEN}✓${NC} build/userscript.ots.user.js"
            ;;
            
        firmware)
            echo -e "${BLUE}Updating firmware version to ${version}${NC}"
            
            # Update config.h
            if [ -f "ots-fw-main/include/config.h" ]; then
                sed -i "s/#define OTS_FIRMWARE_VERSION.*\".*\".*/#define OTS_FIRMWARE_VERSION    \"${version}\"  \/\/ Updated by release.sh/" ots-fw-main/include/config.h
                echo -e "  ${GREEN}✓${NC} ots-fw-main/include/config.h"
            fi
            ;;
            
        server)
            echo -e "${BLUE}Updating server version to ${version}${NC}"
            
            # Update package.json
            if [ -f "ots-server/package.json" ]; then
                sed -i "s/\"version\": \".*\"/\"version\": \"${version}\"/" ots-server/package.json
                echo -e "  ${GREEN}✓${NC} ots-server/package.json"
            fi
            ;;
    esac
}

# Function to show usage
usage() {
    cat << EOF
${GREEN}OTS Release Tagging Script${NC}

Usage: $0 [OPTIONS] PROJECT [PROJECT...]

${YELLOW}Projects:${NC}
  userscript    Tag ots-userscript release
  firmware      Tag ots-fw-main release
  server        Tag ots-server release
  all           Tag all projects

${YELLOW}Options:${NC}
  -m MESSAGE    Release message/description
  -u            Update version numbers in package.json files
  -p            Push tags after creation
  -l            List existing tags for today
  -h            Show this help

${YELLOW}Examples:${NC}
  $0 userscript
  $0 -m "Major bug fixes" userscript server
  $0 -u -p all
  $0 -l

${YELLOW}Tag Format:${NC}
  userscript:  us-YYYY-MM-DD.N
  firmware:    fw-YYYY-MM-DD.N
  server:      sv-YYYY-MM-DD.N

${YELLOW}Notes:${NC}
  - Revision number auto-increments for same-day releases
  - Tags are annotated (include commit info)
  - Use -p to push immediately, or push manually later

EOF
    exit 0
}

# Function to list today's tags
list_today_tags() {
    echo -e "${BLUE}Tags for ${DATE}:${NC}\n"
    
    for prefix in "us-" "fw-" "sv-"; do
        local project=""
        case $prefix in
            "us-") project="Userscript" ;;
            "fw-") project="Firmware  " ;;
            "sv-") project="Server    " ;;
        esac
        
        local tags=$(git tag -l "${prefix}${DATE}.*" | sort -V)
        if [ -z "$tags" ]; then
            echo -e "  ${project}: ${YELLOW}none${NC}"
        else
            echo -e "  ${project}: ${GREEN}$(echo $tags | tr '\n' ' ')${NC}"
        fi
    done
    
    exit 0
}

# Parse command line options
MESSAGE=""
UPDATE_VERSION=false
PUSH_TAGS=false

while getopts "m:uplh" opt; do
    case $opt in
        m)
            MESSAGE="$OPTARG"
            ;;
        u)
            UPDATE_VERSION=true
            ;;
        p)
            PUSH_TAGS=true
            ;;
        l)
            list_today_tags
            ;;
        h)
            usage
            ;;
        \?)
            echo -e "${RED}Invalid option: -$OPTARG${NC}" >&2
            usage
            ;;
    esac
done

shift $((OPTIND-1))

# Check if any projects specified
if [ $# -eq 0 ]; then
    echo -e "${RED}Error: No project specified${NC}"
    usage
fi

# Ensure we're in git repo
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo -e "${RED}Error: Not in a git repository${NC}"
    exit 1
fi

# Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    echo -e "${YELLOW}Warning: You have uncommitted changes${NC}"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo -e "${GREEN}=== OTS Release Tagging ===${NC}"
echo -e "Date: ${BLUE}${DATE}${NC}\n"

CREATED_TAGS=()

# Process each project
for project in "$@"; do
    case $project in
        userscript|us)
            PREFIX="us-"
            DESCRIPTION="${MESSAGE:-Userscript release}"
            REVISION=$(get_next_revision "$PREFIX")
            
            if [ "$UPDATE_VERSION" = true ]; then
                update_version "userscript" "${DATE}.${REVISION}"
            fi
            
            create_tag "Userscript" "$PREFIX" "$DESCRIPTION"
            CREATED_TAGS+=("${PREFIX}${DATE}.${REVISION}")
            ;;
            
        firmware|fw)
            PREFIX="fw-"
            DESCRIPTION="${MESSAGE:-Firmware release}"
            REVISION=$(get_next_revision "$PREFIX")
            
            if [ "$UPDATE_VERSION" = true ]; then
                update_version "firmware" "${DATE}.${REVISION}"
            fi
            
            create_tag "Firmware" "$PREFIX" "$DESCRIPTION"
            CREATED_TAGS+=("${PREFIX}${DATE}.${REVISION}")
            ;;
            
        server|sv)
            PREFIX="sv-"
            DESCRIPTION="${MESSAGE:-Server release}"
            REVISION=$(get_next_revision "$PREFIX")
            
            if [ "$UPDATE_VERSION" = true ]; then
                update_version "server" "${DATE}.${REVISION}"
            fi
            
            create_tag "Server" "$PREFIX" "$DESCRIPTION"
            CREATED_TAGS+=("${PREFIX}${DATE}.${REVISION}")
            ;;
            
        all)
            # Recursively call for all projects
            $0 ${UPDATE_VERSION:+-u} ${PUSH_TAGS:+-p} ${MESSAGE:+-m "$MESSAGE"} userscript firmware server
            exit $?
            ;;
            
        *)
            echo -e "${RED}Unknown project: $project${NC}"
            echo -e "Valid projects: userscript, firmware, server, all"
            exit 1
            ;;
    esac
done

# Push tags if requested
if [ "$PUSH_TAGS" = true ] && [ ${#CREATED_TAGS[@]} -gt 0 ]; then
    echo -e "\n${BLUE}Pushing tags to origin...${NC}"
    for tag in "${CREATED_TAGS[@]}"; do
        git push origin "$tag"
        echo -e "${GREEN}✓${NC} Pushed: $tag"
    done
fi

echo -e "\n${GREEN}=== Release Complete ===${NC}"

if [ "$UPDATE_VERSION" = true ]; then
    echo -e "${YELLOW}Note: Version files updated. Don't forget to commit and push!${NC}"
fi

if [ "$PUSH_TAGS" = false ] && [ ${#CREATED_TAGS[@]} -gt 0 ]; then
    echo -e "\nTo push all tags:"
    echo -e "  ${YELLOW}git push origin --tags${NC}"
    echo -e "\nOr push individual tags:"
    for tag in "${CREATED_TAGS[@]}"; do
        echo -e "  ${YELLOW}git push origin $tag${NC}"
    done
fi
