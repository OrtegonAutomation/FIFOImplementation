import sqlite3
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
    print("\n🔍 Creando índices para optimización...")
    
    # Índice para búsqueda FIFO (por asset y timestamp)
    cursor.execute("""
        CREATE INDEX IF NOT EXISTS idx_fifo_queue 
        ON data_files(asset_id, created_timestamp, status)
    """)
    print("  ✅ Índice FIFO creado")
    
    # Índice para búsqueda por ruta de storage
    cursor.execute("""
        CREATE INDEX IF NOT EXISTS idx_storage_path 
        ON data_files(storage_path_id, status)
    """)
    print("  ✅ Índice de storage creado")
    
    # Índice para log de borrados (por fecha)
    cursor.execute("""
        CREATE INDEX IF NOT EXISTS idx_deletion_timestamp 
        ON deletion_log(deletion_timestamp)
    """)
    print("  ✅ Índice de borrados creado")
    
    conn.commit()
    print("\n✅ Base de datos inicializada correctamente")
    
    # Mostrar información
    cursor.execute("SELECT name FROM sqlite_master WHERE type='table'")
    tables = cursor.fetchall()
    print(f"\n📊 Tablas creadas: {len(tables)}")
    for table in tables:
        print(f"   - {table[0]}")
    
    conn.close()
    return True


def insert_sample_data(db_path):
    """
    Inserta datos de ejemplo para pruebas.
    """
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()
    
    print("\n📝 Insertando datos de ejemplo...")
    
    # Assets de ejemplo
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
    
    # Storage path de ejemplo
    import os
    test_path = os.path.abspath("prototipo\\monitored_storage")
    
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
    import os
    
    # Ruta de la base de datos
    db_path = os.path.join("prototipo", "database", "fifo_prototype.db")
    
    print("=" * 60)
    print("🚀 INICIALIZADOR DE BASE DE DATOS FIFO")
    print("=" * 60)
    
    # Verificar si ya existe
    if os.path.exists(db_path):
        response = input(f"\n⚠️  La base de datos ya existe en:\n{db_path}\n¿Deseas recrearla? (s/n): ")
        if response.lower() != 's':
            print("❌ Operación cancelada")
            exit(0)
        os.remove(db_path)
        print("🗑️  Base de datos anterior eliminada")
    
    # Crear base de datos
    initialize_database(db_path)
    
    # Insertar datos de ejemplo
    response = input("\n¿Deseas insertar datos de ejemplo? (s/n): ")
    if response.lower() == 's':
        insert_sample_data(db_path)
    
    print("\n" + "=" * 60)
    print("✅ PROCESO COMPLETADO")
    print("=" * 60)
    print(f"\n📍 Base de datos ubicada en:\n{os.path.abspath(db_path)}")
    print("\n🔧 Puedes explorar la base de datos con:")
    print("   - DB Browser for SQLite (https://sqlitebrowser.org/)")
    print("   - python -c \"import sqlite3; ...\"")
