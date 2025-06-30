package history.loaders;

import static history.Event.EventType.*;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.nio.file.Path;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.function.BiFunction;
import java.util.stream.Collectors;

import lombok.Data;
import org.apache.commons.lang3.tuple.Pair;

import history.*;
import history.History.*;
import history.Event.EventType;
import lombok.SneakyThrows;
import util.UnimplementedError;

public class ViperHistoryLoader implements HistoryParser<Long, Long> {
	private final File logDir;

	public static long INIT_WRITE_ID = 0xbebeebeeL;
	public static long INIT_TXN_ID = 0xbebeebeeL;
	public static long NULL_TXN_ID = 0xdeadbeefL;
	public static long GC_WID_TRUE = 0x23332333L;
	public static long GC_WID_FALSE = 0x66666666L;

	public ViperHistoryLoader(Path path) {
		logDir = path.toFile();

		if (!logDir.isDirectory()) {
			throw new Error("path is not a directory");
		}
	}

	@Override
	public History<Long, Long> loadHistory() {
		throw new UnimplementedError();
	}

	@Override
	@SneakyThrows
	public void dumpHistory(History<Long, Long> history) {
		if (!logDir.isDirectory()) {
			throw new Error(String.format("%s is not a directory", logDir));
		}
		Arrays.stream(logDir.listFiles()).forEach(f -> f.delete());

		for (var session : history.getSessions()) {
			try (var out = Files.newBufferedWriter(logDir.toPath().resolve(String.format("J%d.log", session.getId())),
        java.nio.charset.StandardCharsets.UTF_8)) { // TODO: +24?

				for (var transaction : session.getTransactions()) {
					out.write("{\"value\": [");

					var is_first = true;
					for (var event : transaction.getEvents()) {
						if (is_first) {
							is_first = false;
						} else {
							out.write(", ");
						}
						out.write(String.format("[\"%s\", %d, %d, true]", 
							event.getType().equals(EventType.READ) ? "r" : "w",
							event.getKey(), event.getValue()));
					}

					out.write("]}");
					out.newLine();
				}
			}
		}
	}

  @Override
  public <T, U> History<Long, Long> convertFrom(History<T, U> history) {
      var events = history.getEvents();
      var keys = Utils.getIdMap(events.stream().map(Event::getKey), 1);
      var values = Utils.getIdMap(events.stream().map(Event::getValue), 1);

      return Utils.convertHistory(history,
              ev -> Pair.of(keys.get(ev.getKey()), values.get(ev.getValue())),
              ev -> true);
  }
}
