Import("env")
import subprocess
import sys
from pathlib import Path

# If error No module named esptool
# C:\Users\WasiliySoft\.platformio\penv\Scripts\python.exe -m pip install esptool

def debug_env():
    """Вывод всех переменных окружения"""
    print("🔍 Все переменные окружения PlatformIO:")
    print("=" * 50)
    
    # Сортируем для удобства чтения
    for key in sorted(env.Dictionary()):
        value = env.get(key)
        # Обрезаем слишком длинные значения
        if isinstance(value, (list, tuple)) and len(str(value)) > 100:
            value = f"[list with {len(value)} items]"
        elif isinstance(value, str) and len(value) > 100:
            value = value[:100] + "..."
        
        print(f"{key}: {value}")
    
    print("=" * 50)

# Добавить эту строку для вывода всех переменных
# debug_env()


def find_filesystem_bin(build_dir):
    """Поиск бинарного файла файловой системы"""
    fs_files = {
        "littlefs": build_dir / "littlefs.bin",
        "spiffs": build_dir / "spiffs.bin"
    }
    
    for fs_type, fs_path in fs_files.items():
        if fs_path.exists():
            return fs_path, fs_type
    
    raise FileNotFoundError("❌ Не найден образ файловой системы (spiffs.bin или littlefs.bin)")

def get_platform_config(platform):
    """Получение конфигурации для конкретной платформы"""
    configs = {
        "espressif32": {
            "chip": "esp32",
            "files": [
                ("bootloader.bin", "0x1000", "Bootloader"),
                ("partitions.bin", "0x8000", "Partitions"),
                ("firmware.bin", "0x10000", "Firmware")
            ],
            "fs_address": "0x210000"
        },
        "espressif8266": {
            "chip": "esp8266", 
            "files": [
                ("firmware.bin", "0x00000", "Firmware")
            ],
            "fs_address": "0x200000"
        }
    }
    
    return configs.get(platform)

def merge_firmware(source, target, env):
    """Основная функция объединения прошивки"""
    # Выполнять только при сборке или прошивке
    if COMMAND_LINE_TARGETS and not any(t in COMMAND_LINE_TARGETS for t in ["build", "upload"]):
        return

    try:
        # Определение платформы
        platform = env.get("PIOPLATFORM")
        platform_config = get_platform_config(platform)
        
        if not platform_config:
            print(f"⚠️  Платформа {platform} не поддерживается")
            return

        # Сборка файловой системы только для текущего окружения
        print("🔨 Сборка файловой системы...")
        env_name = env.get("PIOENV")
        env.Execute(f"pio run -t buildfs -e {env_name}")

        # Пути и параметры
        build_dir = Path(env.subst("$BUILD_DIR"))
        project_name = env.subst("$PROGNAME")
        
        # Создаем отдельный файл для объединенного образа
        merged_bin = build_dir / f"{project_name}_full.bin"

        # Поиск файла файловой системы
        fs_bin, fs_type = find_filesystem_bin(build_dir)
        print(f"📁 Используется ФС: {fs_type.upper()} → {fs_bin.name}")

        # Проверка наличия всех необходимых файлов
        missing_files = []
        cmd_parts = []
        
        # Добавляем основные файлы прошивки
        for file_name, address, description in platform_config["files"]:
            file_path = build_dir / file_name
            if not file_path.exists():
                missing_files.append(f"❌ Не найден: {description} → {file_path}")
            else:
                cmd_parts.extend([address, str(file_path)])

        # Добавляем файловую систему
        if not fs_bin.exists():
            missing_files.append(f"❌ Не найден: {fs_type.upper()} → {fs_bin}")
        else:
            cmd_parts.extend([platform_config["fs_address"], str(fs_bin)])

        # Проверяем наличие всех файлов
        if missing_files:
            print("\n".join(missing_files))
            raise FileNotFoundError("Один или несколько обязательных файлов отсутствуют")

        # Формируем команду esptool
        cmd = [
            sys.executable, "-m", "esptool",
            "--chip", platform_config["chip"],
            "merge-bin",
            "-o", str(merged_bin)
        ] + cmd_parts

        print(f"🔗 Объединение образов для {platform_config['chip'].upper()}...")
        subprocess.run(cmd, check=True)

        # Завершение
        print(f"✅ Полный образ создан: {merged_bin}")
        
        if "upload" in COMMAND_LINE_TARGETS:
                print("🚀 Автоматическая прошивка объединенного образа...")
                upload_port = env.get("UPLOAD_PORT")
                upload_speed = env.get("UPLOAD_SPEED")
                upload_cmd = [
                    sys.executable, "-m", "esptool",
                    "--chip", platform_config["chip"],
                    "--port", upload_port,
                    "--baud", f"{upload_speed}",
                    "write-flash", "0x0", str(merged_bin)
                ]
                subprocess.run(upload_cmd, check=True)
                # ПРЕДОТВРАЩАЕМ стандартную прошивку PlatformIO
                print("✅ Объединенная прошивка завершена. Отмена стандартной прошивки.")
                sys.exit(0)  # Завершаем процесс


    except subprocess.CalledProcessError as e:
        print(f"❌ Ошибка при выполнении esptool: returncode={e.returncode}")
        raise
    except Exception as e:
        print(f"❌ Ошибка при создании полного образа: {e}")
        raise

# Регистрация post-action для поддерживаемых платформ
platform = env.get("PIOPLATFORM")
if platform in ["espressif32", "espressif8266"]:
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_firmware)