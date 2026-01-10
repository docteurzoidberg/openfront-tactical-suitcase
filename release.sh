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
    local pattern="${DATE}."
    
    # Get all tags matching today's date
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
    local description=$1
    local projects=$2
    
    local revision=$(get_next_revision)
    local tag="${DATE}.${revision}"
    
    echo -e "${BLUE}Creating release tag:${NC} ${GREEN}${tag}${NC}"
    echo -e "${BLUE}Projects: ${projects}${NC}"
    
    # Create annotated tag
    git tag -a "$tag" -m "OTS Release ${DATE}.${revision}

${description}

Projects: ${projects}

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
            echo -e "${BLUE}Updating ots-userscript version to ${version}${NC}"
            
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
            ;;
            
        firmware)
            echo -e "${BLUE}Updating ots-fw-main version to ${version}${NC}"
            
            # Update config.h
            if [ -f "ots-fw-main/include/config.h" ]; then
                sed -i "s/#define OTS_FIRMWARE_VERSION.*\".*\".*/#define OTS_FIRMWARE_VERSION    \"${version}\"  \/\/ Updated by release.sh/" ots-fw-main/include/config.h
                echo -e "  ${GREEN}✓${NC} ots-fw-main/include/config.h"
            fi
            ;;
            
        audiomodule)
            echo -e "${BLUE}Updating ots-fw-audiomodule version to ${version}${NC}"
            
            # Update main.c
            if [ -f "ots-fw-audiomodule/src/main.c" ]; then
                sed -i "s/#define AUDIO_MODULE_VERSION.*\".*\".*/#define AUDIO_MODULE_VERSION \"${version}\"/" ots-fw-audiomodule/src/main.c
                echo -e "  ${GREEN}✓${NC} ots-fw-audiomodule/src/main.c"
            fi
            ;;
            
        website)
            echo -e "${BLUE}Updating ots-website version to ${version}${NC}"
            
            # Update package.json
            if [ -f "ots-website/package.json" ]; then
                sed -i "s/\"version\": \".*\"/\"version\": \"${version}\"/" ots-website/package.json
                echo -e "  ${GREEN}✓${NC} ots-website/package.json"
            fi
            ;;
            
        server)
            echo -e "${BLUE}Updating ots-simulator version to ${version}${NC}"
            
            # Update package.json
            if [ -f "ots-simulator/package.json" ]; then
                sed -i "s/\"version\": \".*\"/\"version\": \"${version}\"/" ots-simulator/package.json
                echo -e "  ${GREEN}✓${NC} ots-simulator/package.json"
            fi
            ;;
    esac
}

# Function to build projects
build_project() {
    local project=$1
    
    case $project in
        userscript)
            echo -e "${BLUE}Building ots-userscript...${NC}"
            local build_output=$(cd ots-userscript && npm run build 2>&1)
            if [ $? -eq 0 ]; then
                echo -e "  ${GREEN}✓${NC} ots-userscript/build/userscript.ots.user.js"
                return 0
            else
                echo -e "  ${RED}✗${NC} ots-userscript build failed!"
                echo -e "${RED}Build output:${NC}"
                echo "$build_output" | tail -30
                return 1
            fi
            ;;
            
        firmware)
            echo -e "${BLUE}Building ots-fw-main...${NC}"
            local build_output=$(cd ots-fw-main && pio run -e esp32-s3-dev 2>&1)
            if [ $? -eq 0 ]; then
                echo -e "  ${GREEN}✓${NC} ots-fw-main/.pio/build/esp32-s3-dev/firmware.bin"
                return 0
            else
                echo -e "  ${RED}✗${NC} ots-fw-main build failed!"
                echo -e "${RED}Build output:${NC}"
                echo "$build_output" | tail -30
                return 1
            fi
            ;;
            
        server)
            echo -e "${BLUE}Building ots-simulator...${NC}"
            local build_output=$(cd ots-simulator && npm run build 2>&1)
            if [ $? -eq 0 ]; then
                echo -e "  ${GREEN}✓${NC} ots-simulator/.output/"
                return 0
            else
                echo -e "  ${RED}✗${NC} ots-simulator build failed!"
                echo -e "${RED}Build output:${NC}"
                echo "$build_output" | tail -30
                return 1
            fi
            ;;
            
        audiomodule)
            echo -e "${BLUE}Building ots-fw-audiomodule...${NC}"
            local build_output=$(cd ots-fw-audiomodule && pio run -e esp32-a1s-espidf 2>&1)
            if [ $? -eq 0 ]; then
                echo -e "  ${GREEN}✓${NC} ots-fw-audiomodule/.pio/build/esp32-a1s-espidf/firmware.bin"
                return 0
            else
                echo -e "  ${RED}✗${NC} ots-fw-audiomodule build failed!"
                echo -e "${RED}Build output:${NC}"
                echo "$build_output" | tail -30
                return 1
            fi
            ;;
            
        website)
            echo -e "${BLUE}Building ots-website...${NC}"
            local build_output=$(cd ots-website && npm run build 2>&1)
            if [ $? -eq 0 ]; then
                echo -e "  ${GREEN}✓${NC} ots-website/.vitepress/dist/"
                return 0
            else
                echo -e "  ${RED}✗${NC} ots-website build failed!"
                echo -e "${RED}Build output:${NC}"
                echo "$build_output" | tail -30
                return 1
            fi
            ;;
    esac
}

# Stage release artifacts into the VitePress public/ folder so GitHub Pages can host them.
# Note: This intentionally runs only during -u releases (when we create a release commit).
stage_website_artifacts() {
    local version=$1
    local project=$2

    if [ "$UPDATE_VERSION" != true ]; then
        return 0
    fi

    if [ ! -d "ots-website" ]; then
        echo -e "  ${YELLOW}⚠${NC} ots-website not found; skipping website artifact staging"
        return 0
    fi

    case "$project" in
        userscript)
            local src_js="ots-userscript/build/userscript.ots.user.js"
            if [ ! -f "$src_js" ]; then
                echo -e "  ${RED}✗${NC} Userscript build output not found: ${src_js}"
                return 1
            fi

            local website_base="ots-website/public/releases"
            local dest_dir_version="${website_base}/${version}/userscript"
            local dest_dir_latest="${website_base}/latest/userscript"

            mkdir -p "$dest_dir_version" "$dest_dir_latest"

            local dest_version_js="${dest_dir_version}/userscript-${version}.user.js"
            local dest_latest_js="${dest_dir_latest}/userscript.user.js"

            cp -f "$src_js" "$dest_version_js"
            cp -f "$src_js" "$dest_latest_js"

            echo -e "  ${GREEN}✓${NC} Staged userscript for website: ${dest_version_js}"
            echo -e "  ${GREEN}✓${NC} Updated website latest: ${dest_latest_js}"
            ;;

        firmware)
            local src_bin="ots-fw-main/.pio/build/esp32-s3-dev/firmware.bin"
            if [ ! -f "$src_bin" ]; then
                echo -e "  ${RED}✗${NC} Firmware binary not found: ${src_bin}"
                return 1
            fi

            local website_base="ots-website/public/releases"
            local dest_dir_version="${website_base}/${version}/firmware"
            local dest_dir_latest="${website_base}/latest/firmware"

            mkdir -p "$dest_dir_version" "$dest_dir_latest"

            local dest_version_bin="${dest_dir_version}/ots-fw-main-${version}.bin"
            local dest_latest_bin="${dest_dir_latest}/ots-fw-main.bin"

            cp -f "$src_bin" "$dest_version_bin"
            cp -f "$src_bin" "$dest_latest_bin"

            echo -e "  ${GREEN}✓${NC} Staged firmware for website: ${dest_version_bin}"
            echo -e "  ${GREEN}✓${NC} Updated website latest: ${dest_latest_bin}"
            ;;

        audiomodule)
            local src_bin="ots-fw-audiomodule/.pio/build/esp32-a1s-espidf/firmware.bin"
            if [ ! -f "$src_bin" ]; then
                echo -e "  ${RED}✗${NC} Audio module firmware binary not found: ${src_bin}"
                return 1
            fi

            local website_base="ots-website/public/releases"
            local dest_dir_version="${website_base}/${version}/firmware"
            local dest_dir_latest="${website_base}/latest/firmware"

            mkdir -p "$dest_dir_version" "$dest_dir_latest"

            local dest_version_bin="${dest_dir_version}/ots-fw-audiomodule-${version}.bin"
            local dest_latest_bin="${dest_dir_latest}/ots-fw-audiomodule.bin"

            cp -f "$src_bin" "$dest_version_bin"
            cp -f "$src_bin" "$dest_latest_bin"

            echo -e "  ${GREEN}✓${NC} Staged audio-module firmware for website: ${dest_version_bin}"
            echo -e "  ${GREEN}✓${NC} Updated website latest: ${dest_latest_bin}"
            ;;
    esac
}

# Function to show usage
usage() {
    echo -e "${GREEN}OTS Release Tagging Script${NC}"
    echo ""
    echo "Usage: $0 [OPTIONS] PROJECT [PROJECT...]"
    echo ""
    echo -e "${YELLOW}Projects:${NC}"
    echo "  ots-userscript      Update userscript version"
    echo "  ots-fw-main         Update main firmware version"
    echo "  ots-fw-audiomodule  Update audio module firmware version"
    echo "  ots-simulator       Update dashboard/server version"
    echo "  ots-website         Update documentation site version"
    echo "  all                 Update all projects (default if none specified)"
    echo ""
    echo -e "${YELLOW}Options:${NC}"
    echo "  -m MESSAGE    Release message/description"
    echo "  -u            Update version numbers in all files"
    echo "  -p            Push tags after creation"
    echo "  -l            List existing tags for today"
    echo "  -h            Show this help"
    echo ""
    echo -e "${YELLOW}Examples:${NC}"
    echo "  $0 -u -m \"Bug fixes\""
    echo "  $0 -u -p -m \"Major release\""
    echo "  $0 -l"
    echo ""
    echo -e "${YELLOW}Tag Format:${NC}"
    echo "  YYYY-MM-DD.N   (e.g., 2025-12-20.1, 2025-12-20.2)"
    echo ""
    echo -e "${YELLOW}Notes:${NC}"
    echo "  - One unified tag per release for all projects"
    echo "  - Revision number auto-increments for same-day releases"
    echo "  - Tags are annotated (include commit info)"
    echo "  - Use -p to push immediately, or push manually later"
    echo "  - Specify projects to update only specific versions"
    echo ""
    exit 0
}

# Function to list today's tags
list_today_tags() {
    echo -e "${BLUE}Tags for ${DATE}:${NC}\n"
    
    local tags=$(git tag -l "${DATE}.*" | sort -V)
    if [ -z "$tags" ]; then
        echo -e "  ${YELLOW}none${NC}"
    else
        for tag in $tags; do
            echo -e "  ${GREEN}${tag}${NC}"
            git tag -n1 "$tag" | sed 's/^/    /'
        done
    fi
    
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

# Default to 'all' if no projects specified
if [ $# -eq 0 ]; then
    set -- "all"
fi

# Ensure we're in git repo
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo -e "${RED}Error: Not in a git repository${NC}"
    exit 1
fi

# Check for uncommitted changes (only warn if not updating versions)
if [ "$UPDATE_VERSION" = false ] && ! git diff-index --quiet HEAD --; then
    echo -e "${YELLOW}Warning: You have uncommitted changes${NC}"
    echo -e "${YELLOW}Note: Use -u flag to update versions and commit automatically${NC}"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo -e "${GREEN}=== OTS Release Tagging ===${NC}"
echo -e "Date: ${BLUE}${DATE}${NC}\n"

# Collect projects to process
PROJECTS_TO_UPDATE=()
PROJECT_NAMES=""

for project in "$@"; do
    case $project in
        ots-userscript|userscript|us)
            PROJECTS_TO_UPDATE+=("userscript")
            PROJECT_NAMES="${PROJECT_NAMES}ots-userscript, "
            ;;
        ots-fw-main|firmware|fw)
            PROJECTS_TO_UPDATE+=("firmware")
            PROJECT_NAMES="${PROJECT_NAMES}ots-fw-main, "
            ;;
        ots-simulator|server|sv)
            PROJECTS_TO_UPDATE+=("server")
            PROJECT_NAMES="${PROJECT_NAMES}ots-simulator, "
            ;;
        ots-fw-audiomodule|audiomodule|am)
            PROJECTS_TO_UPDATE+=("audiomodule")
            PROJECT_NAMES="${PROJECT_NAMES}ots-fw-audiomodule, "
            ;;
        ots-website|website|ws)
            PROJECTS_TO_UPDATE+=("website")
            PROJECT_NAMES="${PROJECT_NAMES}ots-website, "
            ;;
        all)
            PROJECTS_TO_UPDATE=("userscript" "firmware" "audiomodule" "server" "website")
            PROJECT_NAMES="ots-userscript, ots-fw-main, ots-fw-audiomodule, ots-simulator, ots-website"
            break
            ;;
        *)
            echo -e "${RED}Unknown project: $project${NC}"
            echo -e "Valid projects: ots-userscript, ots-fw-main, ots-fw-audiomodule, ots-simulator, ots-website, all"
            exit 1
            ;;
    esac
done

# Remove trailing comma and space
PROJECT_NAMES="${PROJECT_NAMES%, }"

# Get the revision number and calculate release version
REVISION=$(get_next_revision)
RELEASE_VERSION="${DATE}.${REVISION}"
DEV_VERSION="${DATE}.$((REVISION + 1))-dev"

# Strip -dev suffix if present in source files (shouldn't be after first run)
VERSION="${RELEASE_VERSION}"

echo -e "${BLUE}Release version: ${GREEN}${VERSION}${NC}"
echo -e "${BLUE}Projects: ${PROJECT_NAMES}${NC}\n"

# Update versions if requested
if [ "$UPDATE_VERSION" = true ]; then
    echo -e "${YELLOW}Updating versions...${NC}\n"
    for project in "${PROJECTS_TO_UPDATE[@]}"; do
        update_version "$project" "$VERSION"
    done
    echo ""
fi

# Build all projects
echo -e "${YELLOW}Building projects...${NC}\n"
BUILD_FAILED=false

for project in "${PROJECTS_TO_UPDATE[@]}"; do
    if ! build_project "$project"; then
        BUILD_FAILED=true
        echo -e "${RED}Build failed for ${project}${NC}"
        break
    fi

    # Stage artifacts right after successful builds so they're included in the release commit.
    if ! stage_website_artifacts "$VERSION" "$project"; then
        BUILD_FAILED=true
        echo -e "${RED}Artifact staging failed for ${project}${NC}"
        break
    fi
done

echo ""

# Check if any build failed
if [ "$BUILD_FAILED" = true ]; then
    echo -e "${RED}✗ Release aborted due to build failure${NC}"
    echo -e "${YELLOW}Note: Version files were updated but tag was not created${NC}"
    echo -e "${YELLOW}Tip: Fix the build errors and run the release script again${NC}"
    exit 1
fi

echo -e "${GREEN}✓ All builds successful${NC}\n"

# Commit version changes and build artifacts
if [ "$UPDATE_VERSION" = true ]; then
    echo -e "${YELLOW}Committing version changes...${NC}"
    
    # Add all modified files
    git add -A
    
    # Create commit message
    COMMIT_MSG="chore(release): version ${VERSION}

Projects: ${PROJECT_NAMES}
${DESCRIPTION}"
    
    if git commit -m "$COMMIT_MSG" > /dev/null 2>&1; then
        echo -e "  ${GREEN}✓${NC} Changes committed"
    else
        echo -e "  ${YELLOW}⚠${NC} No changes to commit (may already be committed)"
    fi
    echo ""
fi

# Create single unified tag
DESCRIPTION="${MESSAGE:-OTS release}"
create_tag "$DESCRIPTION" "$PROJECT_NAMES"

CREATED_TAG="${VERSION}"

# Bump to next dev version after successful release
if [ "$UPDATE_VERSION" = true ]; then
    echo -e "\n${YELLOW}Bumping to development version...${NC}"
    
    for project in "${PROJECTS_TO_UPDATE[@]}"; do
        update_version "$project" "$DEV_VERSION"
    done
    
    # Commit dev version bump
    git add -A
    if git commit -m "chore: bump to dev version ${DEV_VERSION}" > /dev/null 2>&1; then
        echo -e "  ${GREEN}✓${NC} Bumped to ${DEV_VERSION}"
    fi
    echo ""
fi

# Push tag if requested
if [ "$PUSH_TAGS" = true ] && [ -n "$CREATED_TAG" ]; then
    echo -e "\n${BLUE}Pushing commit and tag to origin...${NC}"
    
    # Push commit first
    if git push origin HEAD > /dev/null 2>&1; then
        echo -e "  ${GREEN}✓${NC} Pushed commit"
    else
        echo -e "  ${YELLOW}⚠${NC} Failed to push commit (may already be pushed)"
    fi
    
    # Then push tag
    if git push origin "$CREATED_TAG" > /dev/null 2>&1; then
        echo -e "  ${GREEN}✓${NC} Pushed tag: $CREATED_TAG"
    else
        echo -e "  ${YELLOW}⚠${NC} Failed to push tag"
    fi
fi

echo -e "\n${GREEN}=== Release Complete ===${NC}"

if [ "$UPDATE_VERSION" = true ]; then
    echo -e "${GREEN}✓ Version files committed${NC}"
fi

if [ "$PUSH_TAGS" = false ] && [ -n "$CREATED_TAG" ]; then
    echo -e "\nTo push commit and tag:"
    echo -e "  ${YELLOW}git push origin HEAD${NC}"
    echo -e "  ${YELLOW}git push origin $CREATED_TAG${NC}"
fi