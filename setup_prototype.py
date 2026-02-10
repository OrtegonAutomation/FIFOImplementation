"""
Script de configuración inicial del prototipo.
Crea la estructura de directorios y archivos necesarios.
"""

import os

# Contenido de los archivos a crear
DB_INIT_CONTENT = '''import sqlite3
import os

def initialize_database(db_path):
    """
    Inicializa la base de datos SQLite con el esquema completo.
    
    Args:
        db_path: Ruta al archivo de base de datos
    """
    
    # Crear directorio si no existe
    os.makedirs(os.path.dirname(db_path), exist_ok=True)
    
    # Conectar (crea el archivo si no existe)
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    print("📦 Creando tablas...")
    
    # TABLA 1: Configuración de Assets
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS assets (
            asset_id TEXT PRIMARY KEY,
            asset_name TEXT NOT NULL,
            average_weight_mb REAL NOT NULL DEFAULT 50.0,
            minimum_retention_hours INTEGER NOT NULL DEFAULT 24,
            maximum_retention_days INTEGER NOT NULL DEFAULT 30,
            priority TEXT NOT NULL DEFAULT 'NORMAL',
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    """)
    print("  ✅ Tabla 'assets' creada")
    
    # TABLA 2: Rutas de almacenamiento monitoreadas
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS storage_paths (
            path_id INTEGER PRIMARY KEY AUTOINCREMENT,
            storage_path TEXT NOT NULL UNIQUE,
            total_capacity_gb REAL NOT NULL,
            warning_threshold REAL NOT NULL DEFAULT 0.75,
            critical_threshold REAL NOT NULL DEFAULT 0.90,
            is_monitored BOOLEAN NOT NULL DEFAULT 1,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    """)
    print("  ✅ Tabla 'storage_paths' creada")
    
    # TABLA 3: Registro de archivos (Cola FIFO)
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS data_files (
            file_id INTEGER PRIMARY KEY AUTOINCREMENT,
            asset_id TEXT NOT NULL,
            storage_path_id INTEGER NOT NULL,
            file_path TEXT NOT NULL UNIQUE,
            file_size_mb REAL NOT NULL,
            created_timestamp TIMESTAMP NOT NULL,
            ingested_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            status TEXT NOT NULL DEFAULT 'ACTIVE',
            deleted_timestamp TIMESTAMP,
            FOREIGN KEY (asset_id) REFERENCES assets(asset_id),
            FOREIGN KEY (storage_path_id) REFERENCES storage_paths(path_id)
        )
    """)
    print("  ✅ Tabla 'data_files' creada")
    
    # TABLA 4: Log de borrados (Auditoría)
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS deletion_log (
            log_id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_id INTEGER NOT NULL,
            asset_id TEXT NOT NULL,
            file_path TEXT NOT NULL,
            deletion_reason TEXT NOT NULL,
            space_freed_mb REAL NOT NULL,
            deletion_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            triggered_by TEXT NOT NULL,
            FOREIGN KEY (file_id) REFERENCES data_files(file_id),
            FOREIGN KEY (asset_id) REFERENCES assets(asset_id)
        )
    """)
    print("  ✅ Tabla 'deletion_log' creada")
    
    # ÍNDICES para consultas rápidas FIFO
    print("\\n🔍 Creando índices para optimización...")
    
    cursor.execute("""
        CREATE INDEX IF NOT EXISTS idx_fifo_queue 
        ON data_files(asset_id, created_timestamp, status)
    """)
    print("  ✅ Índice FIFO creado")
    
    cursor.execute("""
        CREATE INDEX IF NOT EXISTS idx_storage_path 
        ON data_files(storage_path_id, status)
    """)
    print("  ✅ Índice de storage creado")
    
    cursor.execute("""
        CREATE INDEX IF NOT EXISTS idx_deletion_timestamp 
        ON deletion_log(deletion_timestamp)
    """)
    print("  ✅ Índice de borrados creado")
    
    conn.commit()
    print("\\n✅ Base de datos inicializada correctamente")
    
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table'")
    tables = cursor.fetchall()
    print(f"\\n📊 Tablas creadas: {len(tables)}")
    for table in tables:
        print(f"   - {table[0]}")
    
    conn.close()
    return True


def insert_sample_data(db_path):
    """Inserta datos de ejemplo para pruebas."""
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    print("\\n📝 Insertando datos de ejemplo...")
    
    sample_assets = [
        ('ASSET_001', 'Sensor de Temperatura - Línea 1', 50.0, 2, 7, 'HIGH'),
        ('ASSET_002', 'Monitor de Presión - Línea 2', 75.0, 4, 14, 'NORMAL'),
        ('ASSET_003', 'Controlador PLC - Línea 3', 30.0, 1, 7, 'HIGH'),
    ]
    
    cursor.executemany("""
        INSERT OR IGNORE INTO assets 
        (asset_id, asset_name, average_weight_mb, minimum_retention_hours, 
         maximum_retention_days, priority)
        VALUES (?, ?, ?, ?, ?, ?)
    """, sample_assets)
    
    print(f"  ✅ {cursor.rowcount} assets insertados")
    
    test_path = os.path.abspath("prototipo\\\\monitored_storage")
    
    cursor.execute("""
        INSERT OR IGNORE INTO storage_paths 
        (storage_path, total_capacity_gb, warning_threshold, critical_threshold)
        VALUES (?, ?, ?, ?)
    """, (test_path, 1.0, 0.75, 0.90))
    
    print(f"  ✅ Storage path configurado: {test_path}")
    
    conn.commit()
    conn.close()
    print("✅ Datos de ejemplo insertados")


if __name__ == "__main__":
    db_path = os.path.join("prototipo", "database", "fifo_prototype.db")
    
    print("=" * 60)
    print("🚀 INICIALIZADOR DE BASE DE DATOS FIFO")
    print("=" * 60)
    
    if os.path.exists(db_path):
        response = input(f"\\n⚠️  La base de datos ya existe en:\\n{db_path}\\n¿Deseas recrearla? (s/n): ")
        if response.lower() != 's':
            print("❌ Operación cancelada")
            exit(0)
        os.remove(db_path)
        print("🗑️  Base de datos anterior eliminada")
    
    initialize_database(db_path)
    
    response = input("\\n¿Deseas insertar datos de ejemplo? (s/n): ")
    if response.lower() == 's':
        insert_sample_data(db_path)
    
    print("\\n" + "=" * 60)
    print("✅ PROCESO COMPLETADO")
    print("=" * 60)
    print(f"\\n📍 Base de datos ubicada en:\\n{os.path.abspath(db_path)}")
'''

def create_file(path, content):
    """Crea un archivo con el contenido especificado."""
    try:
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"   ✅ {path}")
        return True
    except Exception as e:
        print(f"   ❌ Error creando {path}: {e}")
        return False

def setup_prototype():
    """Crea la estructura de directorios del prototipo."""
    
    print("=" * 70)
    print("🚀 CONFIGURACIÓN COMPLETA DEL PROTOTIPO FIFO")
    print("=" * 70)
    
    # Directorios a crear
    directories = [
        "prototipo",
        "prototipo\\config",
        "prototipo\\database",
        "prototipo\\logs",
        "prototipo\\monitored_storage",
        "prototipo\\monitored_storage\\ASSET_001",
        "prototipo\\monitored_storage\\ASSET_002",
        "prototipo\\monitored_storage\\ASSET_003",
    ]
    
    print("\\n📁 Paso 1: Creando estructura de directorios...")
    
    for directory in directories:
        try:
            os.makedirs(directory, exist_ok=True)
            print(f"   ✅ {directory}")
        except Exception as e:
            print(f"   ❌ Error creando {directory}: {e}")
    
    print("\\n📝 Paso 2: Copiando archivos del prototipo...")
    print("\\n   Los archivos deben ser creados desde el código fuente:")
    print("   - prototipo\\db_init.py")
    print("   - prototipo\\fifo_engine.py")
    print("   - prototipo\\synthetic_data_generator.py")
    print("   - prototipo\\test_fifo_complete.py")
    
    print("\\n✅ Estructura de directorios creada correctamente")
    print("\\n📂 Estructura final:")
    print("""
prototipo\\
├── config\\
├── database\\
├── logs\\
├── monitored_storage\\
│   ├── ASSET_001\\
│   ├── ASSET_002\\
│   └── ASSET_003\\
├── db_init.py
├── fifo_engine.py
├── synthetic_data_generator.py
└── test_fifo_complete.py
    """)
    
    print("\\n" + "=" * 70)
    print("✅ CONFIGURACIÓN COMPLETADA")
    print("=" * 70)
    
    print("\\n💡 Siguientes pasos:")
    print("\\n1. Los archivos .py del prototipo ya fueron generados en el")
    print("   documento 'Guia_Implementacion_Paso_a_Paso.md'")
    print("\\n2. Copia el código de cada archivo desde la guía al directorio")
    print("   correspondiente, O ejecuta el siguiente comando para")
    print("   crear el archivo db_init.py automáticamente:")
    print("\\n   Revisa la guía para los contenidos completos")
    print("\\n3. Luego ejecuta:")
    print("   python prototipo\\\\db_init.py")
    print("\\n4. Finalmente ejecuta la prueba:")
    print("   python prototipo\\\\test_fifo_complete.py")
    print("")

if __name__ == "__main__":
    setup_prototype()
