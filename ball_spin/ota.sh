#!/bin/bash
# =============================================================================
# Ball Spin OTA 工具
# =============================================================================
#
# 使用方式:
#   ./ota.sh                    # 互動式選單
#   ./ota.sh -u                 # 快速更新 (最新韌體)
#   ./ota.sh -u -y              # 自動確認更新
#   ./ota.sh -b                 # 編譯後更新
#   ./ota.sh -s                 # 掃描裝置
#   ./ota.sh -v                 # 查看版本
#   ./ota.sh -h                 # 顯示說明
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
OTA_SCRIPT="$SCRIPT_DIR/scripts/ota_updater.py"
FIRMWARE_PATH="$PROJECT_DIR/.pio/build/m5stack-atoms3/firmware.bin"

# 顏色定義
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# =============================================================================
# 命令列參數處理
# =============================================================================

show_usage() {
    echo -e "${BOLD}Ball Spin OTA 工具${NC}"
    echo ""
    echo -e "${CYAN}使用方式:${NC}"
    echo "  ./ota.sh                    # 互動式選單"
    echo "  ./ota.sh [選項]             # 命令列模式"
    echo ""
    echo -e "${CYAN}選項:${NC}"
    echo "  -u, --update          更新韌體 (使用最新建置)"
    echo "  -y, --yes             自動確認 (跳過提示)"
    echo "  -b, --build           編譯後更新"
    echo "  -s, --scan            掃描裝置"
    echo "  -v, --version         查看韌體版本"
    echo "  -h, --help            顯示此說明"
    echo ""
    echo -e "${CYAN}範例:${NC}"
    echo "  ./ota.sh -u             # 更新最新韌體"
    echo "  ./ota.sh -u -y          # 自動確認更新"
    echo "  ./ota.sh -b -y          # 編譯後自動更新"
    echo ""
}

# 命令列模式
run_cli() {
    local do_update=false
    local do_build=false
    local do_scan=false
    local do_version=false
    local auto_yes=false

    while [[ $# -gt 0 ]]; do
        case $1 in
            -u|--update)
                do_update=true
                shift
                ;;
            -y|--yes)
                auto_yes=true
                shift
                ;;
            -b|--build)
                do_build=true
                do_update=true
                shift
                ;;
            -s|--scan)
                do_scan=true
                shift
                ;;
            -v|--version)
                do_version=true
                shift
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            *)
                echo -e "${RED}未知選項: $1${NC}"
                show_usage
                exit 1
                ;;
        esac
    done

    # 執行操作
    if $do_scan; then
        python3 "$OTA_SCRIPT"
        exit $?
    fi

    if $do_version; then
        python3 "$OTA_SCRIPT" --version
        exit $?
    fi

    if $do_build; then
        echo -e "${BLUE}編譯韌體中...${NC}\n"
        cd "$PROJECT_DIR"
        pio run
        if [ $? -ne 0 ]; then
            echo -e "${RED}編譯失敗${NC}"
            exit 1
        fi
        echo -e "${GREEN}編譯成功！${NC}\n"
        cd "$SCRIPT_DIR"
    fi

    if $do_update; then
        if [ ! -f "$FIRMWARE_PATH" ]; then
            echo -e "${RED}錯誤: 找不到韌體檔案${NC}"
            echo -e "路徑: $FIRMWARE_PATH"
            echo -e "\n請先執行 ${YELLOW}pio run${NC} 編譯韌體"
            exit 1
        fi

        local args="--update --latest"
        if $auto_yes; then
            args="$args --yes"
        fi

        python3 "$OTA_SCRIPT" $args
        exit $?
    fi

    # 無有效操作，顯示說明
    show_usage
    exit 1
}

# 檢查是否有命令列參數
if [ $# -gt 0 ]; then
    run_cli "$@"
    exit $?
fi

# =============================================================================
# 互動式選單模式
# =============================================================================

# 顯示標題
show_header() {
    clear
    echo -e "${CYAN}${BOLD}"
    echo "============================================================"
    echo "  Ball Spin OTA 工具"
    echo "============================================================"
    echo -e "${NC}"
}

# 顯示選單
show_menu() {
    echo -e "${BOLD}請選擇操作：${NC}\n"
    echo -e "  ${GREEN}1)${NC} 快速更新 - 自動確認更新最新韌體"
    echo -e "  ${GREEN}2)${NC} 掃描裝置 - 顯示附近的 Ball Spin"
    echo -e "  ${GREEN}3)${NC} 查看版本 - 連接並顯示韌體版本"
    echo -e "  ${GREEN}4)${NC} 更新韌體 - 手動確認更新"
    echo -e "  ${GREEN}5)${NC} 指定檔案 - 更新指定韌體檔案"
    echo -e "  ${GREEN}6)${NC} 編譯並更新 - 先編譯韌體再更新"
    echo -e "  ${GREEN}7)${NC} 僅編譯 - 編譯韌體但不更新"
    echo ""
    echo -e "  ${YELLOW}i)${NC} 安裝依賴 - 安裝 Python 套件"
    echo -e "  ${YELLOW}h)${NC} 說明 - 顯示使用說明"
    echo ""
    echo -e "  ${RED}0)${NC} 離開"
    echo ""
}

# 檢查 Python 依賴
check_dependencies() {
    python3 -c "import bleak" 2>/dev/null
    return $?
}

# 安裝依賴
install_dependencies() {
    echo -e "${BLUE}正在安裝依賴...${NC}"
    pip install bleak
    echo ""
    echo -e "${GREEN}安裝完成！${NC}"
}

# 快速更新 (自動確認)
quick_update() {
    if [ ! -f "$FIRMWARE_PATH" ]; then
        echo -e "${RED}錯誤: 找不到韌體檔案${NC}"
        echo -e "路徑: $FIRMWARE_PATH"
        echo -e "\n請先執行 ${YELLOW}pio run${NC} 編譯韌體"
        return 1
    fi

    local size=$(du -h "$FIRMWARE_PATH" | cut -f1)
    echo -e "${BLUE}快速更新模式 (自動確認)${NC}"
    echo -e "韌體: $FIRMWARE_PATH ($size)"
    echo ""

    python3 "$OTA_SCRIPT" --update --latest --yes
}

# 掃描裝置
scan_devices() {
    echo -e "${BLUE}掃描中...${NC}\n"
    python3 "$OTA_SCRIPT"
}

# 查看版本
show_version() {
    echo -e "${BLUE}查詢韌體版本...${NC}\n"
    python3 "$OTA_SCRIPT" --version
}

# 更新韌體 (最新)
update_latest() {
    if [ ! -f "$FIRMWARE_PATH" ]; then
        echo -e "${RED}錯誤: 找不到韌體檔案${NC}"
        echo -e "路徑: $FIRMWARE_PATH"
        echo -e "\n請先執行 ${YELLOW}pio run${NC} 編譯韌體"
        return 1
    fi

    local size=$(du -h "$FIRMWARE_PATH" | cut -f1)
    local date=$(stat -f "%Sm" "$FIRMWARE_PATH" 2>/dev/null || stat -c "%y" "$FIRMWARE_PATH")

    echo -e "${BLUE}韌體資訊:${NC}"
    echo -e "  檔案: $FIRMWARE_PATH"
    echo -e "  大小: $size"
    echo -e "  修改時間: $date"
    echo ""

    python3 "$OTA_SCRIPT" --update --latest
}

# 更新韌體 (指定檔案)
update_custom() {
    echo -e "${BLUE}請輸入韌體檔案路徑:${NC}"
    read -e -p "> " firmware_file

    if [ -z "$firmware_file" ]; then
        echo -e "${RED}已取消${NC}"
        return 1
    fi

    # 展開路徑
    firmware_file="${firmware_file/#\~/$HOME}"

    if [ ! -f "$firmware_file" ]; then
        echo -e "${RED}錯誤: 找不到檔案 '$firmware_file'${NC}"
        return 1
    fi

    python3 "$OTA_SCRIPT" --update "$firmware_file"
}

# 編譯韌體
build_firmware() {
    echo -e "${BLUE}編譯韌體中...${NC}\n"
    cd "$PROJECT_DIR"
    pio run
    local result=$?
    cd "$SCRIPT_DIR"

    if [ $result -eq 0 ]; then
        echo ""
        echo -e "${GREEN}編譯成功！${NC}"
        local size=$(du -h "$FIRMWARE_PATH" | cut -f1)
        echo -e "韌體大小: $size"
    else
        echo -e "${RED}編譯失敗${NC}"
    fi

    return $result
}

# 編譯並更新
build_and_update() {
    build_firmware
    if [ $? -eq 0 ]; then
        echo ""
        echo -e "${BLUE}準備更新...${NC}\n"
        python3 "$OTA_SCRIPT" --update --latest
    fi
}

# 顯示說明
show_help() {
    echo -e "${BOLD}Ball Spin OTA 工具使用說明${NC}"
    echo ""
    echo -e "${CYAN}功能說明:${NC}"
    echo "  此工具透過 BLE (藍牙低功耗) 連接 Ball Spin 裝置，"
    echo "  可以查看韌體版本並進行無線韌體更新 (OTA)。"
    echo ""
    echo -e "${CYAN}更新流程:${NC}"
    echo "  1. 確保 Ball Spin 裝置已開機"
    echo "  2. 掃描並連接裝置"
    echo "  3. 傳輸韌體資料"
    echo "  4. 驗證完成後自動重啟"
    echo ""
    echo -e "${CYAN}注意事項:${NC}"
    echo "  - 更新期間請勿斷電"
    echo "  - 建議距離裝置 2 公尺內"
    echo ""
    echo -e "${CYAN}Shell 命令列模式:${NC}"
    echo "  ./ota.sh -u             # 更新最新韌體"
    echo "  ./ota.sh -u -y          # 自動確認更新"
    echo "  ./ota.sh -b -y          # 編譯後自動更新"
    echo "  ./ota.sh -s             # 掃描裝置"
    echo "  ./ota.sh -v             # 查看版本"
    echo "  ./ota.sh -h             # 顯示說明"
}

# 等待按鍵
wait_key() {
    echo ""
    echo -e "${YELLOW}按任意鍵繼續...${NC}"
    read -n 1 -s
}

# 主程式
main() {
    # 檢查 ota_updater.py 是否存在
    if [ ! -f "$OTA_SCRIPT" ]; then
        echo -e "${RED}錯誤: 找不到 ota_updater.py${NC}"
        exit 1
    fi

    while true; do
        show_header

        # 檢查依賴狀態
        if ! check_dependencies; then
            echo -e "${YELLOW}提示: 尚未安裝 Python 依賴，請先執行選項 i${NC}\n"
        fi

        show_menu

        read -n 1 -p "請輸入選項 [0-7,i,h]: " choice
        echo -e "\n"

        case $choice in
            1)
                quick_update
                wait_key
                ;;
            2)
                scan_devices
                wait_key
                ;;
            3)
                show_version
                wait_key
                ;;
            4)
                update_latest
                wait_key
                ;;
            5)
                update_custom
                wait_key
                ;;
            6)
                build_and_update
                wait_key
                ;;
            7)
                build_firmware
                wait_key
                ;;
            i|I)
                install_dependencies
                wait_key
                ;;
            h|H)
                show_help
                wait_key
                ;;
            0|q|Q)
                echo -e "${GREEN}再見！${NC}"
                exit 0
                ;;
            *)
                echo -e "${RED}無效選項，請重新輸入${NC}"
                sleep 1
                ;;
        esac
    done
}

# 執行互動式選單
main
