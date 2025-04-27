<script lang="ts">
	import '../app.css';
	import '@picocss/pico';

	let active = $state(false);
	function toggle() {
		active = !active;
	}
	let { children } = $props();


</script>

<div class="app">
	<main>
			<div class="continer">
				{@render children()}
		</div>
	</main>

	<footer>
		<p>
			Created by <a href="https://github.com/Damarioo" target="_blank">Damario Sujatmiko</a>
		</p>
	</footer>
</div>

<style>
	.app {
		display: flex;
		flex-direction: column;
		min-height: 100vh;
	}

	main {
		flex: 1;
		display: flex;
		flex-direction: column;
		padding: 1rem;
		width: 100%;
		max-width: 64rem;
		margin: 0 auto;
		box-sizing: border-box;
	}

	footer {
		display: flex;
		flex-direction: column;
		justify-content: center;
		align-items: center;
		padding: 12px;
	}

	footer a {
		font-weight: bold;
	}

	@media (min-width: 480px) {
		footer {
			padding: 12px 0;
		}
	}

	aside {
		position: absolute;
		left: -500px;
		transition: all .5s;
		padding: 20px;
		height: 100vh;
	}
	.active {
		left: 0px
	}
</style>